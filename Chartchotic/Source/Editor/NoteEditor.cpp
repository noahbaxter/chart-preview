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

    auto existing = midiWriter->findNote(trackIdx, startQN, pitch);
    if (existing.noteIndex >= 0 && existing.startQN < startQN)
    {
        if (batchActive)
            midiWriter->batchMoveNote(trackIdx, existing.noteIndex, existing.startQN, startQN, pitch);
        else
            midiWriter->moveNote(trackIdx, existing.noteIndex, existing.startQN, startQN, pitch);
    }

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

    auto note = midiWriter->findNote(trackIdx, rawQN, pitch);

    if (note.noteIndex < 0 && drums && lane == 0)
    {
        int kick2xPitch = InstrumentMapper::columnToDrumPitch(skill, 0, true);
        note = midiWriter->findNote(trackIdx, rawQN, kick2xPitch);
    }

    if (note.noteIndex < 0) return false;

    bool ok = batchActive
        ? midiWriter->batchDeleteNote(trackIdx, note.noteIndex, rawQN)
        : midiWriter->deleteNoteAtQN(trackIdx, note.noteIndex, rawQN);

    if (ok) instrumentSession->invalidateTrack(trackIdx);
    return ok;
}

bool NoteEditor::truncateNote(int trackIdx, double noteStartQN, int pitch)
{
    if (!midiWriter || !instrumentSession) return false;

    auto note = midiWriter->findNote(trackIdx, noteStartQN, pitch);
    if (note.noteIndex < 0) return false;

    double shortEnd = note.startQN + kShortNoteDurationQN;
    bool ok = batchActive
        ? midiWriter->batchMoveNote(trackIdx, note.noteIndex, note.startQN, shortEnd, pitch)
        : midiWriter->moveNote(trackIdx, note.noteIndex, note.startQN, shortEnd, pitch);

    if (ok) instrumentSession->invalidateTrack(trackIdx);
    return ok;
}

bool NoteEditor::extendNote(int trackIdx, double startQN, double endQN, int pitch)
{
    if (!midiWriter || !instrumentSession) return false;

    auto note = midiWriter->findNote(trackIdx, startQN, pitch);
    if (note.noteIndex < 0) return false;

    bool ok = batchActive
        ? midiWriter->batchMoveNote(trackIdx, note.noteIndex, note.startQN, endQN, pitch)
        : midiWriter->moveNote(trackIdx, note.noteIndex, note.startQN, endQN, pitch);

    if (ok) instrumentSession->invalidateTrack(trackIdx);
    return ok;
}

bool NoteEditor::chainExtendNotes(int trackIdx, double startQN, double endQN, int pitch)
{
    if (!midiWriter || !instrumentSession) return false;

    auto notes = midiWriter->findNotesInRange(trackIdx, startQN, endQN, pitch);
    if (notes.empty()) return false;

    bool changed = false;
    for (int i = (int)notes.size() - 1; i >= 0; --i)
    {
        double noteEnd = (i + 1 < (int)notes.size()) ? notes[i + 1].startQN : endQN;
        if (noteEnd <= notes[i].startQN) continue;

        auto note = midiWriter->findNote(trackIdx, notes[i].startQN, pitch);
        if (note.noteIndex < 0) continue;

        bool ok = batchActive
            ? midiWriter->batchMoveNote(trackIdx, note.noteIndex, note.startQN, noteEnd, pitch)
            : midiWriter->moveNote(trackIdx, note.noteIndex, note.startQN, noteEnd, pitch);

        if (ok) changed = true;
    }

    if (changed) instrumentSession->invalidateTrack(trackIdx);
    return changed;
}

MidiWriter::NoteInfo NoteEditor::findNote(int trackIdx, double qn, int pitch)
{
    if (!midiWriter) return {};
    return midiWriter->findNote(trackIdx, qn, pitch);
}

bool NoteEditor::moveNote(int trackIdx, double oldStartQN, int oldPitch,
                          double newStartQN, double newEndQN, int newPitch)
{
    if (!midiWriter || !instrumentSession) return false;

    auto note = midiWriter->findNote(trackIdx, oldStartQN, oldPitch);
    if (note.noteIndex < 0) return false;

    bool ok = batchActive
        ? midiWriter->batchMoveNote(trackIdx, note.noteIndex, newStartQN, newEndQN, newPitch)
        : midiWriter->moveNote(trackIdx, note.noteIndex, newStartQN, newEndQN, newPitch);

    if (ok) instrumentSession->invalidateTrack(trackIdx);
    return ok;
}

std::vector<MidiWriter::NoteInfo> NoteEditor::findNotesInRange(int trackIdx, double startQN,
                                                                double endQN, int pitch)
{
    if (!midiWriter) return {};
    return midiWriter->findNotesInRange(trackIdx, startQN, endQN, pitch);
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
