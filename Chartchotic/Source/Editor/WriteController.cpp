#include "WriteController.h"
#include "../PluginProcessor.h"
#include "../Visual/HighwayComponent.h"
#include "../Midi/Utils/InstrumentMapper.h"
#include "../UI/ControlConstants.h"

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

void WriteController::toggle()
{
    auto* writer = processor->reaperMidiProvider.getWriter();
    if (!writer)
        return;

    active = !active;
    int trackIndex = (int)state->getProperty("reaperTrack") - 1;

    highway->setWriteMode(active, writer, trackIndex);

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
    highway->isDrawMode = (mode == InteractionMode::DRAW);
    highway->drawModeSnapEnabled = snapEnabled;

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

    // Drag complete: DRAW = sustain placement
    highway->onDragComplete = [this](double startTime, int startLane, double endTime, int endLane) {
        if (mode != InteractionMode::DRAW || endLane < 0) return;

        auto* w = processor->reaperMidiProvider.getWriter();
        if (!w) return;

        int pitch = pitchForLane(endLane);
        if (pitch < 0) return;

        int trackIdx = (int)state->getProperty("reaperTrack") - 1;
        double cursorTimeSec = processor->reaperMidiProvider.getCurrentCursorPosition();

        double startPPQ = processor->reaperMidiProvider.timeToPpq(cursorTimeSec + startTime);
        double endPPQ = processor->reaperMidiProvider.timeToPpq(cursorTimeSec + endTime);
        if (startPPQ < 0.0) startPPQ = 0.0;

        startPPQ = snapToGrid(startPPQ);
        endPPQ = snapToGrid(endPPQ);

        // Only allow sustain if dragging forward in time
        // Sustain minimum: resolution/3 PPQ (≈160 ticks at 480 PPQ)
        constexpr double SUSTAIN_MIN_PPQ = 1.0 / 3.0;  // 1/3 of a quarter note (160 ticks at 480 PPQ)
        double duration = endPPQ - startPPQ;

        if (duration >= SUSTAIN_MIN_PPQ)
        {
            w->insertNote(trackIdx, startPPQ, endPPQ, 0, pitch, 100);
        }
        else
        {
            // Short note
            double shortEnd = startPPQ + 0.0625;
            w->insertNote(trackIdx, startPPQ, shortEnd, 0, pitch, 100);
        }

        // Draw mode: no selection state (selection is an Edit mode concept)
    };

    // Key actions: controller reads selection and acts
    highway->onKeyAction = [this](int action) {
        using KA = HighwayComponent::KeyAction;

        if (action == KA::KA_DELETE)
        {
            if (!selection.hasSelection()) return;
            auto* w = processor->reaperMidiProvider.getWriter();
            if (!w) return;
            int trackIdx = (int)state->getProperty("reaperTrack") - 1;
            int matchIdx = findNoteIndexByPPQ(selection.ppq, selection.pitch);
            if (matchIdx >= 0)
                w->deleteNote(trackIdx, matchIdx);
            return;
        }

        if (action == KA::KA_MOVE_UP || action == KA::KA_MOVE_DOWN)
        {
            if (!selection.hasSelection()) return;
            auto* w = processor->reaperMidiProvider.getWriter();
            if (!w) return;
            int trackIdx = (int)state->getProperty("reaperTrack") - 1;
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
            int trackIdx = (int)state->getProperty("reaperTrack") - 1;
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
    highway->onKeyAction = nullptr;
}

int WriteController::findNoteIndex(double timeFromCursor, int pitch, double& outPPQ)
{
    double cursorTimeSec = processor->reaperMidiProvider.getCurrentCursorPosition();
    double absoluteTimeSec = cursorTimeSec + timeFromCursor;
    double ppq = processor->reaperMidiProvider.timeToPpq(absoluteTimeSec);
    if (ppq < 0.0) ppq = 0.0;
    outPPQ = ppq;

    int trackIdx = (int)state->getProperty("reaperTrack") - 1;
    auto allNotes = processor->reaperMidiProvider.getAllNotesFromTrack(trackIdx);
    constexpr double PPQ_TOLERANCE = 0.25;
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

void WriteController::selectFromHit(double timeFromCursor, int lane)
{
    int pitch = pitchForLane(lane);
    if (pitch < 0) return;

    double cursorTimeSec = processor->reaperMidiProvider.getCurrentCursorPosition();
    double absoluteTimeSec = cursorTimeSec + timeFromCursor;
    double ppq = processor->reaperMidiProvider.timeToPpq(absoluteTimeSec);

    selection = { ppq, pitch, lane };
}

void WriteController::placeNote(double timeFromCursor, int lane)
{
    auto* w = processor->reaperMidiProvider.getWriter();
    if (!w) return;

    int pitch = pitchForLane(lane);
    if (pitch < 0) return;

    int trackIdx = (int)state->getProperty("reaperTrack") - 1;
    double cursorTimeSec = processor->reaperMidiProvider.getCurrentCursorPosition();
    double ppq = processor->reaperMidiProvider.timeToPpq(cursorTimeSec + timeFromCursor);
    if (ppq < 0.0) ppq = 0.0;
    ppq = snapToGrid(ppq);

    double endPPQ = ppq + 0.0625;
    w->insertNote(trackIdx, ppq, endPPQ, 0, pitch, 100);
    selection = { ppq, pitch, lane };
}

void WriteController::eraseNote(double timeFromCursor, int lane)
{
    auto* w = processor->reaperMidiProvider.getWriter();
    if (!w) return;

    int pitch = pitchForLane(lane);
    if (pitch < 0) return;

    int trackIdx = (int)state->getProperty("reaperTrack") - 1;
    double ppq;
    int matchIdx = findNoteIndex(timeFromCursor, pitch, ppq);
    if (matchIdx >= 0)
        w->deleteNote(trackIdx, matchIdx);
}

int WriteController::findNoteIndexByPPQ(double ppq, int pitch)
{
    int trackIdx = (int)state->getProperty("reaperTrack") - 1;
    auto allNotes = processor->reaperMidiProvider.getAllNotesFromTrack(trackIdx);
    constexpr double PPQ_TOLERANCE = 0.25;
    for (int i = 0; i < (int)allNotes.size(); i++)
    {
        if (allNotes[i].pitch == pitch &&
            std::abs(allNotes[i].startPPQ - ppq) < PPQ_TOLERANCE)
            return i;
    }
    return -1;
}

// --- Sub-mode ---

void WriteController::toggleMode()
{
    mode = (mode == InteractionMode::DRAW) ? InteractionMode::EDIT : InteractionMode::DRAW;
}

// --- Grid config ---

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
