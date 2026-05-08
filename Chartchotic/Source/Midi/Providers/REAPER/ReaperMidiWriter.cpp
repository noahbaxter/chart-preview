/*
  ==============================================================================

    ReaperMidiWriter.cpp
    REAPER implementation of MidiWriter

  ==============================================================================
*/

#include "ReaperMidiWriter.h"
#include "ReaperApiHelpers.h"

#include <algorithm>

ReaperMidiWriter::ReaperMidiWriter(const ReaperAPIs& apis,
                                   std::function<void*(const char*)> reaperGetFunc)
    : apis(apis), getReaperApi(std::move(reaperGetFunc)),
      itemManager(apis, getReaperApi)
{
}

bool ReaperMidiWriter::isAvailable() const
{
    return apis.writeApisLoaded() && getReaperApi != nullptr;
}

void ReaperMidiWriter::endUndoBlock(void* project, const char* description)
{
    if (apis.MarkProjectDirty) apis.MarkProjectDirty(project);
    if (apis.Undo_OnStateChange) apis.Undo_OnStateChange(description);
}

void ReaperMidiWriter::addBatchTake(void* take)
{
    if (std::find(batchTakes.begin(), batchTakes.end(), take) != batchTakes.end())
        return;
    if (apis.MIDI_DisableSort) apis.MIDI_DisableSort(take);
    batchTakes.push_back(take);
}

// =============================================================================
// Single-note operations
// =============================================================================

bool ReaperMidiWriter::insertNote(int trackIndex, double startQN, double endQN,
                                  int channel, int pitch, int velocity)
{
    juce::ScopedLock lock(writeLock);
    void* project = ReaperApiHelpers::getProject(getReaperApi);
    if (!project) return false;

    void* take = itemManager.getTakeForWrite(project, trackIndex, startQN, endQN);
    if (!take) return false;

    double takePPQStart = apis.MIDI_GetPPQPosFromProjQN(take, startQN);
    double takePPQEnd   = apis.MIDI_GetPPQPosFromProjQN(take, endQN);

    bool ok = apis.MIDI_InsertNote(take, true, false,
                                   takePPQStart, takePPQEnd,
                                   channel, pitch, velocity, nullptr);
    if (apis.MIDI_Sort) apis.MIDI_Sort(take);
    endUndoBlock(project, "Chartchotic: Insert note");
    return ok;
}

MidiWriter::NoteInfo ReaperMidiWriter::findNote(int trackIndex, double positionQN, int pitch)
{
    void* project = ReaperApiHelpers::getProject(getReaperApi);
    if (!project) return {};
    return itemManager.findNote(project, trackIndex, positionQN, pitch);
}

std::vector<MidiWriter::NoteInfo>
ReaperMidiWriter::findNotesInRange(int trackIndex, double startQN,
                                    double endQN, int pitch)
{
    void* project = ReaperApiHelpers::getProject(getReaperApi);
    if (!project) return {};
    return itemManager.findNotesInRange(project, trackIndex, startQN, endQN, pitch);
}

bool ReaperMidiWriter::deleteNote(int trackIndex, int noteIndex)
{
    return deleteNoteAtQN(trackIndex, noteIndex, -1.0);
}

bool ReaperMidiWriter::deleteNoteAtQN(int trackIndex, int noteIndex, double hintQN)
{
    juce::ScopedLock lock(writeLock);
    void* project = ReaperApiHelpers::getProject(getReaperApi);
    if (!project) return false;

    void* take = itemManager.getLastFoundTake();
    if (!take) return false;

    bool ok = apis.MIDI_DeleteNote(take, noteIndex);
    if (apis.MIDI_Sort) apis.MIDI_Sort(take);
    endUndoBlock(project, "Chartchotic: Delete note");
    return ok;
}

bool ReaperMidiWriter::moveNote(int trackIndex, int noteIndex,
                                double newStartQN, double newEndQN, int newPitch)
{
    juce::ScopedLock lock(writeLock);
    void* project = ReaperApiHelpers::getProject(getReaperApi);
    if (!project) return false;

    void* take = itemManager.getLastFoundTake();
    if (!take) return false;

    double takePPQStart = apis.MIDI_GetPPQPosFromProjQN(take, newStartQN);
    double takePPQEnd   = apis.MIDI_GetPPQPosFromProjQN(take, newEndQN);

    bool ok = apis.MIDI_SetNote(take, noteIndex,
                                nullptr, nullptr,
                                &takePPQStart, &takePPQEnd,
                                nullptr, &newPitch,
                                nullptr, nullptr);
    if (apis.MIDI_Sort) apis.MIDI_Sort(take);
    endUndoBlock(project, "Chartchotic: Move note");
    return ok;
}

// =============================================================================
// Batch operations
// =============================================================================

void ReaperMidiWriter::beginBatch(const char* undoDescription)
{
    juce::ScopedLock lock(writeLock);
    batchProject = ReaperApiHelpers::getProject(getReaperApi);
    if (!batchProject) return;

    batchDescription = undoDescription ? undoDescription : "Chartchotic: Batch edit";
    inBatch = true;
    batchTakes.clear();
}

bool ReaperMidiWriter::batchInsertNote(int trackIndex, double startQN, double endQN,
                                       int channel, int pitch, int velocity)
{
    juce::ScopedLock lock(writeLock);
    if (!inBatch || !batchProject) return false;

    void* take = itemManager.getTakeForWrite(batchProject, trackIndex, startQN, endQN);
    if (!take) return false;
    addBatchTake(take);

    double takePPQStart = apis.MIDI_GetPPQPosFromProjQN(take, startQN);
    double takePPQEnd   = apis.MIDI_GetPPQPosFromProjQN(take, endQN);

    const bool noSort = true;
    return apis.MIDI_InsertNote(take, true, false,
                                takePPQStart, takePPQEnd,
                                channel, pitch, velocity, &noSort);
}

bool ReaperMidiWriter::batchDeleteNote(int trackIndex, int noteIndex, double hintQN)
{
    juce::ScopedLock lock(writeLock);
    if (!inBatch || !batchProject) return false;

    void* take = itemManager.getLastFoundTake();
    if (!take) return false;
    addBatchTake(take);

    return apis.MIDI_DeleteNote(take, noteIndex);
}

bool ReaperMidiWriter::batchMoveNote(int trackIndex, int noteIndex,
                                     double newStartQN, double newEndQN, int newPitch)
{
    juce::ScopedLock lock(writeLock);
    if (!inBatch || !batchProject) return false;

    void* take = itemManager.getLastFoundTake();
    if (!take) return false;
    addBatchTake(take);

    double takePPQStart = apis.MIDI_GetPPQPosFromProjQN(take, newStartQN);
    double takePPQEnd   = apis.MIDI_GetPPQPosFromProjQN(take, newEndQN);

    const bool noSort = true;
    return apis.MIDI_SetNote(take, noteIndex,
                             nullptr, nullptr,
                             &takePPQStart, &takePPQEnd,
                             nullptr, &newPitch,
                             nullptr, &noSort);
}

void ReaperMidiWriter::endBatch()
{
    juce::ScopedLock lock(writeLock);
    if (!inBatch || !batchProject) return;

    for (auto* take : batchTakes)
        if (apis.MIDI_Sort) apis.MIDI_Sort(take);

    endUndoBlock(batchProject, batchDescription.toRawUTF8());

    inBatch = false;
    batchProject = nullptr;
    batchTakes.clear();
    batchDescription.clear();
}
