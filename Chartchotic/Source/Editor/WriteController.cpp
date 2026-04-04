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
    // Double-click: toggle note (insert or delete)
    highway->onNoteEditRequested = [this](double timeFromCursor, int pitch) {
        auto* w = processor->reaperMidiProvider.getWriter();
        if (!w) return;

        int trackIdx = (int)state->getProperty("reaperTrack") - 1;
        double ppq;
        int matchIdx = findNoteIndex(timeFromCursor, pitch, ppq);

        if (matchIdx >= 0)
        {
            // Delete existing note — keep selection identity so undo restores it
            w->deleteNote(trackIdx, matchIdx);

            int lane = 0;
            std::vector<uint> pitches;
            resolvePitches(pitches);
            for (int i = 0; i < (int)pitches.size(); i++)
                if ((int)pitches[i] == pitch) { lane = i; break; }

            selection = { ppq, pitch, lane };
        }
        else
        {
            // Create new note — select it
            double endPPQ = ppq + 0.0625;
            w->insertNote(trackIdx, ppq, endPPQ, 0, pitch, 100);

            int lane = 0;
            std::vector<uint> pitches;
            resolvePitches(pitches);
            for (int i = 0; i < (int)pitches.size(); i++)
                if ((int)pitches[i] == pitch) { lane = i; break; }

            selection = { ppq, pitch, lane };
        }
    };

    // DELETE key: remove selected note
    highway->onNoteDeleteRequested = [this](double timeFromCursor, int pitch) {
        auto* w = processor->reaperMidiProvider.getWriter();
        if (!w) return;

        int trackIdx = (int)state->getProperty("reaperTrack") - 1;
        double ppq;
        int matchIdx = findNoteIndex(timeFromCursor, pitch, ppq);
        if (matchIdx >= 0)
        {
            w->deleteNote(trackIdx, matchIdx);
            // Keep selection identity — if user undoes, the note reappears
            // and the selection highlight comes back automatically via
            // update(). The highlight won't draw while the note is absent
            // since it's not in the visible trackWindow.
        }
    };

    // Up/Down arrow: shift selected note forward/backward in time
    highway->onNoteMoveRequested = [this](double timeFromCursor, int pitch, int direction) {
        auto* w = processor->reaperMidiProvider.getWriter();
        if (!w) return;

        int trackIdx = (int)state->getProperty("reaperTrack") - 1;
        double ppq;
        int matchIdx = findNoteIndex(timeFromCursor, pitch, ppq);
        if (matchIdx < 0) return;

        auto allNotes = processor->reaperMidiProvider.getAllNotesFromTrack(trackIdx);
        if (matchIdx >= (int)allNotes.size()) return;

        double noteDuration = allNotes[matchIdx].endPPQ - allNotes[matchIdx].startPPQ;
        double shift = 0.25 * (double)direction;  // 1/16th note per step
        double newStart = std::max(0.0, allNotes[matchIdx].startPPQ + shift);
        double newEnd = newStart + noteDuration;
        w->moveNote(trackIdx, matchIdx, newStart, newEnd, pitch);

        // Update selection to track the moved note
        selection.ppq = newStart;
    };

    // Single-click: select or deselect
    highway->onNoteClicked = [this](double timeFromCursor, int lane, bool noteExists) {
        if (noteExists)
            selectFromHit(timeFromCursor, lane);
        else
            selection = WriteSelection::none();
    };
}

void WriteController::clearCallbacks()
{
    if (!highway) return;
    highway->onNoteEditRequested = nullptr;
    highway->onNoteDeleteRequested = nullptr;
    highway->onNoteMoveRequested = nullptr;
    highway->onNoteClicked = nullptr;
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
