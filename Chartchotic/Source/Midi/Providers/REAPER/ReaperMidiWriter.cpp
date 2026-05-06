/*
  ==============================================================================

    ReaperMidiWriter.cpp
    REAPER implementation of MidiWriter

  ==============================================================================
*/

#include "ReaperMidiWriter.h"
#include "ReaperApiHelpers.h"

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

// =============================================================================
// Undo helpers
// =============================================================================

void ReaperMidiWriter::beginUndoBlock(void* /*project*/, const char* /*description*/)
{
}

void ReaperMidiWriter::endUndoBlock(void* project, const char* description)
{
    if (apis.MarkProjectDirty)
        apis.MarkProjectDirty(project);
    if (apis.Undo_OnStateChange)
        apis.Undo_OnStateChange(description);
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

    beginUndoBlock(project, "Chartchotic: Insert note");

    double takePPQStart = apis.MIDI_GetPPQPosFromProjQN(take, startQN);
    double takePPQEnd = apis.MIDI_GetPPQPosFromProjQN(take, endQN);

    bool ok = apis.MIDI_InsertNote(take, true, false,
                                   takePPQStart, takePPQEnd,
                                   channel, pitch, velocity, nullptr);

    if (apis.MIDI_Sort)
        apis.MIDI_Sort(take);

    endUndoBlock(project, "Chartchotic: Insert note");
    return ok;
}

bool ReaperMidiWriter::deleteNote(int trackIndex, int noteIndex)
{
    juce::ScopedLock lock(writeLock);

    void* project = ReaperApiHelpers::getProject(getReaperApi);
    if (!project) return false;

    void* take = itemManager.getFirstMidiTake(project, trackIndex);
    if (!take) return false;

    beginUndoBlock(project, "Chartchotic: Delete note");

    bool ok = apis.MIDI_DeleteNote(take, noteIndex);

    if (apis.MIDI_Sort)
        apis.MIDI_Sort(take);

    endUndoBlock(project, "Chartchotic: Delete note");
    return ok;
}

bool ReaperMidiWriter::moveNote(int trackIndex, int noteIndex,
                                double newStartQN, double newEndQN, int newPitch)
{
    juce::ScopedLock lock(writeLock);

    void* project = ReaperApiHelpers::getProject(getReaperApi);
    if (!project) return false;

    void* take = itemManager.getFirstMidiTake(project, trackIndex);
    if (!take) return false;

    beginUndoBlock(project, "Chartchotic: Move note");

    double takePPQStart = apis.MIDI_GetPPQPosFromProjQN(take, newStartQN);
    double takePPQEnd = apis.MIDI_GetPPQPosFromProjQN(take, newEndQN);

    bool ok = apis.MIDI_SetNote(take, noteIndex,
                                nullptr, nullptr,
                                &takePPQStart, &takePPQEnd,
                                nullptr, &newPitch,
                                nullptr, nullptr);

    if (apis.MIDI_Sort)
        apis.MIDI_Sort(take);

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
    batchTake = nullptr;

    beginUndoBlock(batchProject, batchDescription.toRawUTF8());
}

bool ReaperMidiWriter::batchInsertNote(int trackIndex, double startQN, double endQN,
                                       int channel, int pitch, int velocity)
{
    juce::ScopedLock lock(writeLock);
    if (!inBatch || !batchProject) return false;

    void* take = itemManager.getFirstMidiTake(batchProject, trackIndex);
    if (!take) return false;

    if (batchTake != take)
    {
        if (apis.MIDI_DisableSort)
            apis.MIDI_DisableSort(take);
        batchTake = take;
    }

    double takePPQStart = apis.MIDI_GetPPQPosFromProjQN(take, startQN);
    double takePPQEnd = apis.MIDI_GetPPQPosFromProjQN(take, endQN);

    const bool noSort = true;
    return apis.MIDI_InsertNote(take, true, false,
                                takePPQStart, takePPQEnd,
                                channel, pitch, velocity, &noSort);
}

bool ReaperMidiWriter::batchDeleteNote(int trackIndex, int noteIndex)
{
    juce::ScopedLock lock(writeLock);
    if (!inBatch || !batchProject) return false;

    void* take = itemManager.getFirstMidiTake(batchProject, trackIndex);
    if (!take) return false;

    if (batchTake != take)
    {
        if (apis.MIDI_DisableSort)
            apis.MIDI_DisableSort(take);
        batchTake = take;
    }

    return apis.MIDI_DeleteNote(take, noteIndex);
}

bool ReaperMidiWriter::batchMoveNote(int trackIndex, int noteIndex,
                                     double newStartQN, double newEndQN, int newPitch)
{
    juce::ScopedLock lock(writeLock);
    if (!inBatch || !batchProject) return false;

    void* take = itemManager.getFirstMidiTake(batchProject, trackIndex);
    if (!take) return false;

    if (batchTake != take)
    {
        if (apis.MIDI_DisableSort)
            apis.MIDI_DisableSort(take);
        batchTake = take;
    }

    double takePPQStart = apis.MIDI_GetPPQPosFromProjQN(take, newStartQN);
    double takePPQEnd = apis.MIDI_GetPPQPosFromProjQN(take, newEndQN);

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

    if (batchTake && apis.MIDI_Sort)
        apis.MIDI_Sort(batchTake);

    endUndoBlock(batchProject, batchDescription.toRawUTF8());

    inBatch = false;
    batchProject = nullptr;
    batchTake = nullptr;
    batchDescription.clear();
}
