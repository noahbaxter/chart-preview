#include "NoteEditor.h"
#include "AuthoringTypes.h"

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

bool NoteEditor::createNote(int trackIdx, double startQN, int pitch, int velocity)
{
    if (!midiWriter || !instrumentSession) return false;

    double endQN = resolveOverlaps(trackIdx, startQN, startQN + kShortNoteDurationQN, pitch);

    bool ok = batchActive
        ? midiWriter->batchInsertNote(trackIdx, startQN, endQN, 0, pitch, velocity)
        : midiWriter->insertNote(trackIdx, startQN, endQN, 0, pitch, velocity);

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

bool NoteEditor::setNoteVelocity(int trackIdx, double qn, int pitch, int velocity)
{
    if (!midiWriter || !instrumentSession) return false;

    auto note = midiWriter->findNote(trackIdx, qn, pitch);
    if (note.noteIndex < 0) return false;

    double startQN = note.startQN;
    double endQN = note.endQN;

    bool ok = batchActive
        ? midiWriter->batchDeleteNote(trackIdx, note.noteIndex, qn)
        : midiWriter->deleteNoteAtQN(trackIdx, note.noteIndex, qn);
    if (!ok) return false;

    ok = batchActive
        ? midiWriter->batchInsertNote(trackIdx, startQN, endQN, 0, pitch, velocity)
        : midiWriter->insertNote(trackIdx, startQN, endQN, 0, pitch, velocity);

    if (ok) instrumentSession->invalidateTrack(trackIdx);
    return ok;
}

bool NoteEditor::chainExtendNotes(int trackIdx, double startQN, double endQN, int pitch)
{
    if (!midiWriter || !instrumentSession) return false;

    auto prev = midiWriter->findNote(trackIdx, startQN, pitch);
    if (prev.noteIndex >= 0 && prev.startQN < startQN - kQNEpsilon)
    {
        if (batchActive)
            midiWriter->batchMoveNote(trackIdx, prev.noteIndex, prev.startQN, startQN, pitch);
        else
            midiWriter->moveNote(trackIdx, prev.noteIndex, prev.startQN, startQN, pitch);
    }

    auto notes = midiWriter->findNotesInRange(trackIdx, startQN, endQN, pitch);
    if (notes.empty()) return false;

    double lastNoteEnd = endQN;
    auto tail = midiWriter->findNote(trackIdx, endQN, pitch);
    if (tail.noteIndex >= 0 && tail.startQN > notes.back().startQN + kQNEpsilon
        && tail.startQN < endQN + kQNEpsilon)
        lastNoteEnd = tail.startQN;

    bool changed = false;
    for (int i = (int)notes.size() - 1; i >= 0; --i)
    {
        double noteEnd = (i + 1 < (int)notes.size()) ? notes[i + 1].startQN : lastNoteEnd;
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

    newEndQN = resolveOverlaps(trackIdx, newStartQN, newEndQN, newPitch);

    note = midiWriter->findNote(trackIdx, oldStartQN, oldPitch);
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

double NoteEditor::resolveOverlaps(int trackIdx, double startQN, double endQN, int pitch)
{
    if (!midiWriter) return endQN;

    auto prev = midiWriter->findNote(trackIdx, startQN, pitch);
    if (prev.noteIndex >= 0 && prev.startQN < startQN - kQNEpsilon)
    {
        if (batchActive)
            midiWriter->batchMoveNote(trackIdx, prev.noteIndex, prev.startQN, startQN, pitch);
        else
            midiWriter->moveNote(trackIdx, prev.noteIndex, prev.startQN, startQN, pitch);
    }

    auto inRange = midiWriter->findNotesInRange(trackIdx, startQN, endQN, pitch);
    for (const auto& n : inRange)
    {
        if (n.startQN > startQN + kQNEpsilon)
        {
            endQN = n.startQN;
            break;
        }
    }

    return endQN;
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
