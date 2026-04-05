#include "WriteController.h"
#include "../PluginProcessor.h"
#include "../Visual/HighwayComponent.h"
#include "../Midi/Utils/InstrumentMapper.h"
#include "../UI/ControlConstants.h"

namespace {
    constexpr double SUSTAIN_MIN_PPQ  = 1.0 / 3.0;   // Minimum sustain duration (1/3 quarter note)
    constexpr double SHORT_NOTE_PPQ   = 0.0625;       // Default short note length (1/16 quarter note)
    constexpr double PPQ_TOLERANCE    = 0.25;          // Tolerance for note matching by PPQ position
    constexpr int    DEFAULT_VELOCITY = 100;
}

WriteController::~WriteController()
{
    clearCallbacks();
}

void WriteController::init(ChartchoticAudioProcessor& proc,
                           juce::ValueTree& st,
                           HighwayComponent& hw)
{
    processor = &proc;
    state = &st;
    highway = &hw;

    // Restore persisted grid config
    if (st.hasProperty("writeStepDivision"))
        stepDivision = juce::jlimit(1, 64, (int)st.getProperty("writeStepDivision"));
    if (st.hasProperty("writeTuplet"))
        tuplet = (int)st.getProperty("writeTuplet");
    if (st.hasProperty("writeSnap"))
        snapEnabled = (bool)st.getProperty("writeSnap");
}

// =============================================================================
// Common helpers
// =============================================================================

int WriteController::getTrackIndex() const
{
    return (int)state->getProperty("reaperTrack") - 1;
}

double WriteController::timeFromCursorToPPQ(double timeFromCursor) const
{
    double cursorTimeSec = processor->reaperMidiProvider.getCurrentCursorPosition();
    return processor->reaperMidiProvider.timeToPpq(cursorTimeSec + timeFromCursor);
}

// =============================================================================
// Toggle / Update
// =============================================================================

void WriteController::toggle()
{
    auto* writer = processor->reaperMidiProvider.getWriter();
    if (!writer)
        return;

    active = !active;
    highway->setWriteMode(active);

    if (active)
        wireCallbacks();
    else
    {
        clearCallbacks();
        selection = WriteSelection::none();
    }
}

void WriteController::update(bool isPlaying)
{
    if (!active)
        return;

    // Push visual hints to highway
    highway->writeHints.drawMode = (mode == InteractionMode::DRAW);
    highway->writeHints.snapEnabled = snapEnabled;

    // Compute min sustain in normalized position space for preview threshold
    double cursorTime = processor->reaperMidiProvider.getCurrentCursorPosition();
    double cursorPPQ = processor->reaperMidiProvider.timeToPpq(cursorTime);
    if (cursorPPQ >= 0.0)
    {
        double minEndTime = processor->reaperMidiProvider.ppqToTime(cursorPPQ + SUSTAIN_MIN_PPQ);
        double minDurationSec = minEndTime - cursorTime;
        auto& fd = highway->getFrameData();
        double windowSpan = fd.windowEndTime - fd.windowStartTime;
        highway->writeHints.minSustainNormalized = (windowSpan > 0.0)
            ? (float)(minDurationSec / windowSpan) : 0.0f;
    }

    // Clear selection when playback starts
    if (isPlaying && !wasPlaying)
        selection = WriteSelection::none();
    wasPlaying = isPlaying;

    if (!selection.hasSelection())
    {
        highway->clearSelection();
        return;
    }

    // Convert PPQ back to time-from-cursor for the renderer
    double cursorTimeSec = processor->reaperMidiProvider.getCurrentCursorPosition();
    double noteTimeSec = processor->reaperMidiProvider.ppqToTime(selection.ppq);
    double timeFromCursor = noteTimeSec - cursorTimeSec;

    highway->setSelection(timeFromCursor, selection.lane);
}

// =============================================================================
// Callbacks
// =============================================================================

void WriteController::wireCallbacks()
{
    // Left click: DRAW = place note, EDIT = select/deselect
    highway->onLeftClick = [this](double timeFromCursor, int lane, bool noteExists) {
        if (mode == InteractionMode::DRAW)
        {
            if (noteExists || lane < 0) return;  // no-op on existing notes
            placeNote(timeFromCursor, lane);
            selection = WriteSelection::none();
        }
        else // EDIT
        {
            if (noteExists)
                selectFromHit(timeFromCursor, lane);
            else
                selection = WriteSelection::none();
        }
    };

    // Right click: DRAW = erase note
    highway->onRightClick = [this](double timeFromCursor, int lane, bool noteExists) {
        if (mode == InteractionMode::DRAW && noteExists)
            eraseNote(timeFromCursor, lane);
    };

    // Right click on sustain body: DRAW = shorten to short note
    highway->onSustainRightClick = [this](double sustainStartTime, int lane) {
        if (mode == InteractionMode::DRAW)
            shortenSustain(sustainStartTime, lane);
    };

    // Double click: EDIT = place/erase toggle
    highway->onDoubleClick = [this](double timeFromCursor, int lane, bool noteExists) {
        if (mode == InteractionMode::EDIT)
        {
            if (noteExists)
                eraseNote(timeFromCursor, lane);
            else if (lane >= 0)
                placeNote(timeFromCursor, lane);
        }
    };

    // Drag complete: DRAW = sustain placement or adjustment
    highway->onDragComplete = [this](double startTime, int startLane, double endTime, int endLane) {
        if (mode != InteractionMode::DRAW || endLane < 0) return;

        auto* w = processor->reaperMidiProvider.getWriter();
        if (!w) return;

        // Use the current lane (where mouse is now), not where drag started
        int pitch = pitchForLane(endLane);
        if (pitch < 0) return;

        int trackIdx = getTrackIndex();

        double startPPQ = timeFromCursorToPPQ(startTime);
        double endPPQ = timeFromCursorToPPQ(endTime);
        if (startPPQ < 0.0) startPPQ = 0.0;

        startPPQ = snapToGridOrNote(startPPQ, pitch);
        endPPQ = snapToGridOrNote(endPPQ, pitch);

        // Check if dragging from an existing note — cascade sustain through all notes in range
        double existingPPQ;
        int existingIdx = findNoteIndex(startTime, pitch, existingPPQ);
        if (existingIdx >= 0)
        {
            auto allNotes = processor->reaperMidiProvider.getAllNotesFromTrack(trackIdx);
            if (existingIdx < (int)allNotes.size())
                existingPPQ = allNotes[existingIdx].startPPQ;

            // Collect all notes in this pitch between start and drag end (uncapped)
            struct NoteHit { int idx; double ppq; };
            std::vector<NoteHit> hits;
            for (int i = 0; i < (int)allNotes.size(); i++)
            {
                if (allNotes[i].pitch == pitch &&
                    allNotes[i].startPPQ >= existingPPQ - PPQ_TOLERANCE &&
                    allNotes[i].startPPQ <= endPPQ + PPQ_TOLERANCE)
                    hits.push_back({ i, allNotes[i].startPPQ });
            }
            std::sort(hits.begin(), hits.end(), [](const NoteHit& a, const NoteHit& b) {
                return a.ppq < b.ppq;
            });

            w->beginBatch("Chartchotic: Cascade sustain");
            for (size_t h = 0; h < hits.size(); h++)
            {
                double noteStart = hits[h].ppq;
                double noteEnd;
                if (h + 1 < hits.size())
                    noteEnd = hits[h + 1].ppq;  // extend to next note
                else
                    noteEnd = endPPQ;            // last note extends to drag end

                // Cap at next note after this one (per-segment cap)
                double nextAfter = findNextNotePPQ(noteStart, pitch);
                if (nextAfter > 0.0 && noteEnd > nextAfter)
                    noteEnd = nextAfter;

                double dur = noteEnd - noteStart;
                double finalEnd = (dur >= SUSTAIN_MIN_PPQ) ? noteEnd : noteStart + SHORT_NOTE_PPQ;
                w->batchMoveNote(trackIdx, hits[h].idx, noteStart, finalEnd, pitch);
            }
            w->endBatch();
        }
        else
        {
            // New note — cap at next note in lane
            double nextPPQ = findNextNotePPQ(startPPQ, pitch);
            if (nextPPQ > 0.0 && endPPQ > nextPPQ)
                endPPQ = nextPPQ;

            double duration = endPPQ - startPPQ;
            if (duration >= SUSTAIN_MIN_PPQ)
                w->insertNote(trackIdx, startPPQ, endPPQ, 0, pitch, DEFAULT_VELOCITY);
            else
                w->insertNote(trackIdx, startPPQ, startPPQ + SHORT_NOTE_PPQ, 0, pitch, DEFAULT_VELOCITY);
        }
    };

    // Key actions: controller reads selection and acts
    highway->onKeyAction = [this](int action) {
        using KA = HighwayComponent::KeyAction;

        if (action == KA::KA_DELETE)
        {
            if (!selection.hasSelection()) return;
            auto* w = processor->reaperMidiProvider.getWriter();
            if (!w) return;
            int matchIdx = findNoteIndexByPPQ(selection.ppq, selection.pitch);
            if (matchIdx >= 0)
                w->deleteNote(getTrackIndex(), matchIdx);
            return;
        }

        if (action == KA::KA_MOVE_UP || action == KA::KA_MOVE_DOWN)
        {
            if (!selection.hasSelection()) return;
            auto* w = processor->reaperMidiProvider.getWriter();
            if (!w) return;
            int trackIdx = getTrackIndex();
            int matchIdx = findNoteIndexByPPQ(selection.ppq, selection.pitch);
            if (matchIdx < 0) return;

            auto allNotes = processor->reaperMidiProvider.getAllNotesFromTrack(trackIdx);
            if (matchIdx >= (int)allNotes.size()) return;

            double noteDuration = allNotes[matchIdx].endPPQ - allNotes[matchIdx].startPPQ;
            double shift = stepSizeInPPQ() * (action == KA::KA_MOVE_UP ? 1.0 : -1.0);
            double newStart = std::max(0.0, allNotes[matchIdx].startPPQ + shift);
            double newEnd = newStart + noteDuration;
            w->moveNote(trackIdx, matchIdx, newStart, newEnd, selection.pitch);
            selection.ppq = newStart;
            return;
        }

        if (action == KA::KA_LANE_LEFT || action == KA::KA_LANE_RIGHT)
        {
            if (!selection.hasSelection()) return;
            auto* w = processor->reaperMidiProvider.getWriter();
            if (!w) return;

            int dir = (action == KA::KA_LANE_RIGHT) ? 1 : -1;
            int newLane = selection.lane + dir;

            std::vector<uint> pitches;
            resolvePitches(pitches);
            if (newLane < 0 || newLane >= (int)pitches.size()) return;  // stop at edges

            int newPitch = (int)pitches[(size_t)newLane];
            int trackIdx = getTrackIndex();
            int matchIdx = findNoteIndexByPPQ(selection.ppq, selection.pitch);
            if (matchIdx < 0) return;

            auto allNotes = processor->reaperMidiProvider.getAllNotesFromTrack(trackIdx);
            if (matchIdx >= (int)allNotes.size()) return;

            w->moveNote(trackIdx, matchIdx,
                        allNotes[matchIdx].startPPQ, allNotes[matchIdx].endPPQ, newPitch);
            selection.pitch = newPitch;
            selection.lane = newLane;
            return;
        }
    };
}

void WriteController::clearCallbacks()
{
    if (!highway) return;
    highway->onLeftClick = nullptr;
    highway->onRightClick = nullptr;
    highway->onDoubleClick = nullptr;
    highway->onDragComplete = nullptr;
    highway->onSustainRightClick = nullptr;
    highway->onKeyAction = nullptr;
}

// =============================================================================
// Note lookup
// =============================================================================

int WriteController::findNoteIndex(double timeFromCursor, int pitch, double& outPPQ)
{
    double ppq = timeFromCursorToPPQ(timeFromCursor);
    if (ppq < 0.0) ppq = 0.0;
    outPPQ = ppq;
    return findNoteIndexByPPQ(ppq, pitch);
}

int WriteController::findNoteIndexByPPQ(double ppq, int pitch)
{
    auto allNotes = processor->reaperMidiProvider.getAllNotesFromTrack(getTrackIndex());
    for (int i = 0; i < (int)allNotes.size(); i++)
    {
        if (allNotes[i].pitch == pitch &&
            std::abs(allNotes[i].startPPQ - ppq) < PPQ_TOLERANCE)
            return i;
    }
    return -1;
}

void WriteController::resolvePitches(std::vector<uint>& out)
{
    bool isDrums = isDrumLike(highway->getActivePart());
    SkillLevel skill = (SkillLevel)(int)state->getProperty("skillLevel");
    out = isDrums
        ? InstrumentMapper::getDrumPitchesForSkill(skill)
        : InstrumentMapper::getGuitarPitchesForSkill(skill);
}

int WriteController::pitchForLane(int lane)
{
    std::vector<uint> pitches;
    resolvePitches(pitches);
    if (lane < 0 || lane >= (int)pitches.size()) return -1;
    return (int)pitches[(size_t)lane];
}

// =============================================================================
// Note operations
// =============================================================================

void WriteController::selectFromHit(double timeFromCursor, int lane)
{
    int pitch = pitchForLane(lane);
    if (pitch < 0) return;

    double ppq = timeFromCursorToPPQ(timeFromCursor);
    selection = { ppq, pitch, lane };
}

void WriteController::placeNote(double timeFromCursor, int lane)
{
    auto* w = processor->reaperMidiProvider.getWriter();
    if (!w) return;

    int pitch = pitchForLane(lane);
    if (pitch < 0) return;

    double ppq = timeFromCursorToPPQ(timeFromCursor);
    if (ppq < 0.0) ppq = 0.0;
    ppq = snapToGrid(ppq);

    w->insertNote(getTrackIndex(), ppq, ppq + SHORT_NOTE_PPQ, 0, pitch, DEFAULT_VELOCITY);
    selection = { ppq, pitch, lane };
}

void WriteController::eraseNote(double timeFromCursor, int lane)
{
    auto* w = processor->reaperMidiProvider.getWriter();
    if (!w) return;

    int pitch = pitchForLane(lane);
    if (pitch < 0) return;

    double ppq;
    int matchIdx = findNoteIndex(timeFromCursor, pitch, ppq);
    if (matchIdx >= 0)
        w->deleteNote(getTrackIndex(), matchIdx);
}

void WriteController::shortenSustain(double sustainStartTime, int lane)
{
    auto* w = processor->reaperMidiProvider.getWriter();
    if (!w) return;

    int pitch = pitchForLane(lane);
    if (pitch < 0) return;

    double ppq;
    int matchIdx = findNoteIndex(sustainStartTime, pitch, ppq);
    if (matchIdx >= 0)
        w->moveNote(getTrackIndex(), matchIdx, ppq, ppq + SHORT_NOTE_PPQ, pitch);
}

double WriteController::findNextNotePPQ(double afterPPQ, int pitch)
{
    auto allNotes = processor->reaperMidiProvider.getAllNotesFromTrack(getTrackIndex());
    double best = -1.0;
    for (const auto& note : allNotes)
    {
        if (note.pitch == pitch && note.startPPQ > afterPPQ + PPQ_TOLERANCE)
        {
            if (best < 0.0 || note.startPPQ < best)
                best = note.startPPQ;
        }
    }
    return best;
}

// =============================================================================
// Sub-mode
// =============================================================================

void WriteController::toggleMode()
{
    mode = (mode == InteractionMode::DRAW) ? InteractionMode::EDIT : InteractionMode::DRAW;
}

// =============================================================================
// Grid config
// =============================================================================

void WriteController::setStepDivision(int div)
{
    stepDivision = juce::jlimit(1, 64, div);
    if (state) state->setProperty("writeStepDivision", stepDivision, nullptr);
}

void WriteController::halveStepDivision()
{
    if (stepDivision > 1) setStepDivision(stepDivision / 2);
}

void WriteController::doubleStepDivision()
{
    if (stepDivision < 64) setStepDivision(stepDivision * 2);
}

void WriteController::cycleTuplet()
{
    // 0 → 3 → 5 → 7 → 0
    if (tuplet == 0) tuplet = 3;
    else if (tuplet == 3) tuplet = 5;
    else if (tuplet == 5) tuplet = 7;
    else tuplet = 0;
    if (state) state->setProperty("writeTuplet", tuplet, nullptr);
}

void WriteController::setSnapEnabled(bool on)
{
    snapEnabled = on;
    if (state) state->setProperty("writeSnap", snapEnabled, nullptr);
}

double WriteController::stepSizeInPPQ() const
{
    double step = 4.0 / stepDivision;
    if (tuplet >= 3) step *= (static_cast<double>(tuplet - 1) / tuplet);
    return step;
}

double WriteController::snapToGrid(double ppq) const
{
    if (!snapEnabled) return ppq;
    double step = stepSizeInPPQ();
    if (step <= 0.0) return ppq;
    return std::round(ppq / step) * step;
}

double WriteController::snapToGridOrNote(double ppq, int pitch)
{
    double gridSnapped = snapToGrid(ppq);
    double bestDist = std::abs(gridSnapped - ppq);
    double best = gridSnapped;

    auto allNotes = processor->reaperMidiProvider.getAllNotesFromTrack(getTrackIndex());
    for (const auto& note : allNotes)
    {
        if (note.pitch != pitch) continue;
        double dist = std::abs(note.startPPQ - ppq);
        if (dist < bestDist)
        {
            bestDist = dist;
            best = note.startPPQ;
        }
    }
    return best;
}
