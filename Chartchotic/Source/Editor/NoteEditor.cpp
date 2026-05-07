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
    if (!midiWriter->insertNote(trackIdx, startQN, endQN, 0, pitch, 100))
        return false;

    instrumentSession->invalidateTrack(trackIdx);
    return true;
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

bool NoteEditor::extendNote(int trackIdx, double startQN, double endQN, int pitch)
{
    if (!midiWriter || !instrumentSession) return false;

    int idx = midiWriter->findNoteIndex(trackIdx, startQN, pitch);
    if (idx < 0) return false;

    if (!midiWriter->moveNote(trackIdx, idx, startQN, endQN, pitch))
        return false;

    instrumentSession->invalidateTrack(trackIdx);
    return true;
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
