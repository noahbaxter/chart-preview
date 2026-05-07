#include "NoteEditor.h"

#include "../Midi/Providers/MidiWriter.h"
#include "../Midi/InstrumentSession.h"
#include "../Midi/Utils/InstrumentMapper.h"

namespace
{
    constexpr double kShortNoteDurationQN = 0.1;
}

bool NoteEditor::isAvailable() const
{
    return midiWriter != nullptr && midiWriter->isAvailable();
}

bool NoteEditor::createNote(int trackIdx, double startQN, int pitch)
{
    if (!midiWriter || !instrumentSession) return false;

    double endQN = startQN + kShortNoteDurationQN;
    bool ok = batchActive
        ? midiWriter->batchInsertNote(trackIdx, startQN, endQN, 0, pitch, 100)
        : midiWriter->insertNote(trackIdx, startQN, endQN, 0, pitch, 100);

    if (ok) instrumentSession->invalidateTrack(trackIdx);
    return ok;
}

bool NoteEditor::eraseNoteAt(int trackIdx, double rawQN, int pitch,
                             bool drums, int lane, SkillLevel skill)
{
    if (!midiWriter || !instrumentSession) return false;

    int noteIdx = midiWriter->findNoteIndex(trackIdx, rawQN, pitch);

    if (noteIdx < 0 && drums && lane == 0)
    {
        int kick2xPitch = InstrumentMapper::columnToDrumPitch(skill, 0, true);
        noteIdx = midiWriter->findNoteIndex(trackIdx, rawQN, kick2xPitch);
    }

    if (noteIdx < 0) return false;

    bool ok = batchActive
        ? midiWriter->batchDeleteNote(trackIdx, noteIdx, rawQN)
        : midiWriter->deleteNoteAtQN(trackIdx, noteIdx, rawQN);

    if (ok) instrumentSession->invalidateTrack(trackIdx);
    return ok;
}

bool NoteEditor::truncateNote(int trackIdx, double noteStartQN, int pitch)
{
    if (!midiWriter || !instrumentSession) return false;

    int idx = midiWriter->findNoteIndex(trackIdx, noteStartQN, pitch);
    if (idx < 0) return false;

    double shortEnd = noteStartQN + kShortNoteDurationQN;
    bool ok = batchActive
        ? midiWriter->batchMoveNote(trackIdx, idx, noteStartQN, shortEnd, pitch)
        : midiWriter->moveNote(trackIdx, idx, noteStartQN, shortEnd, pitch);

    if (ok) instrumentSession->invalidateTrack(trackIdx);
    return ok;
}

bool NoteEditor::extendNote(int trackIdx, double startQN, double endQN, int pitch)
{
    if (!midiWriter || !instrumentSession) return false;

    int idx = midiWriter->findNoteIndex(trackIdx, startQN, pitch);
    if (idx < 0) return false;

    bool ok = batchActive
        ? midiWriter->batchMoveNote(trackIdx, idx, startQN, endQN, pitch)
        : midiWriter->moveNote(trackIdx, idx, startQN, endQN, pitch);

    if (ok) instrumentSession->invalidateTrack(trackIdx);
    return ok;
}

bool NoteEditor::chainExtendNotes(int trackIdx, double startQN, double endQN, int pitch)
{
    if (!midiWriter || !instrumentSession) return false;

    auto notes = midiWriter->findNotesInRange(trackIdx, startQN, endQN, pitch);
    if (notes.empty()) return false;

    // Process in reverse so extended bodies don't interfere with findNoteIndex
    // lookups for earlier notes (body matching would find the wrong note).
    bool changed = false;
    for (int i = (int)notes.size() - 1; i >= 0; --i)
    {
        double noteEnd = (i + 1 < (int)notes.size()) ? notes[i + 1].startQN : endQN;
        if (noteEnd <= notes[i].startQN) continue;

        int idx = midiWriter->findNoteIndex(trackIdx, notes[i].startQN, pitch);
        if (idx < 0) continue;

        bool ok = batchActive
            ? midiWriter->batchMoveNote(trackIdx, idx, notes[i].startQN, noteEnd, pitch)
            : midiWriter->moveNote(trackIdx, idx, notes[i].startQN, noteEnd, pitch);

        if (ok) changed = true;
    }

    if (changed) instrumentSession->invalidateTrack(trackIdx);
    return changed;
}

int NoteEditor::findNote(int trackIdx, double qn, int pitch)
{
    if (!midiWriter) return -1;
    return midiWriter->findNoteIndex(trackIdx, qn, pitch);
}

void NoteEditor::beginBatch(const char* description)
{
    if (!midiWriter) return;
    midiWriter->beginBatch(description);
    batchActive = true;
}

void NoteEditor::endBatch()
{
    if (midiWriter && batchActive)
        midiWriter->endBatch();
    batchActive = false;
}
