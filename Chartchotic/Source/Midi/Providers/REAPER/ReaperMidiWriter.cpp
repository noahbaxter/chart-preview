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
    : apis(apis), getReaperApi(std::move(reaperGetFunc))
{
}

bool ReaperMidiWriter::isAvailable() const
{
    return apis.writeApisLoaded() && getReaperApi != nullptr;
}

// =============================================================================
// Take resolution
// =============================================================================

void* ReaperMidiWriter::getFirstMidiTake(void* project, int trackIndex)
{
    if (!project || !apis.GetTrack || !apis.CountMediaItems ||
        !apis.GetMediaItem || !apis.GetActiveTake || !apis.GetMediaItemTake_Track)
        return nullptr;

    void* track = apis.GetTrack(project, trackIndex);
    if (!track)
        return nullptr;

    int itemCount = apis.CountMediaItems(project);
    for (int i = 0; i < itemCount; i++)
    {
        void* item = apis.GetMediaItem(project, i);
        if (!item) continue;

        void* take = apis.GetActiveTake(item);
        if (!take) continue;

        if (apis.GetMediaItemTake_Track(take) == track)
        {
            // Verify it's a MIDI take
            int noteCount = 0;
            if (apis.MIDI_CountEvts && apis.MIDI_CountEvts(take, &noteCount, nullptr, nullptr))
                return take;
        }
    }

    return nullptr;
}

// =============================================================================
// Undo helpers
// =============================================================================

void ReaperMidiWriter::beginUndoBlock(void* project, const char* description)
{
    if (apis.Undo_BeginBlock2)
        apis.Undo_BeginBlock2(project);
}

void ReaperMidiWriter::endUndoBlock(void* project, const char* description)
{
    if (apis.MarkProjectDirty)
        apis.MarkProjectDirty(project);

    // extraflags: -1 = all undo state flags
    if (apis.Undo_EndBlock2)
        apis.Undo_EndBlock2(project, description, -1);
}

// =============================================================================
// Single-note operations
// =============================================================================

bool ReaperMidiWriter::insertNote(int trackIndex, double startPPQ, double endPPQ,
                                  int channel, int pitch, int velocity)
{
    juce::ScopedLock lock(writeLock);

    void* project = ReaperApiHelpers::getProject(getReaperApi);
    if (!project) return false;

    void* take = getFirstMidiTake(project, trackIndex);
    if (!take) return false;

    beginUndoBlock(project, "Chartchotic: Insert note");

    // Convert project QN to take PPQ
    double takePPQStart = apis.MIDI_GetPPQPosFromProjQN(take, startPPQ);
    double takePPQEnd = apis.MIDI_GetPPQPosFromProjQN(take, endPPQ);

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

    void* take = getFirstMidiTake(project, trackIndex);
    if (!take) return false;

    beginUndoBlock(project, "Chartchotic: Delete note");

    bool ok = apis.MIDI_DeleteNote(take, noteIndex);

    if (apis.MIDI_Sort)
        apis.MIDI_Sort(take);

    endUndoBlock(project, "Chartchotic: Delete note");
    return ok;
}

bool ReaperMidiWriter::moveNote(int trackIndex, int noteIndex,
                                double newStartPPQ, double newEndPPQ, int newPitch)
{
    juce::ScopedLock lock(writeLock);

    void* project = ReaperApiHelpers::getProject(getReaperApi);
    if (!project) return false;

    void* take = getFirstMidiTake(project, trackIndex);
    if (!take) return false;

    beginUndoBlock(project, "Chartchotic: Move note");

    // Convert project QN to take PPQ
    double takePPQStart = apis.MIDI_GetPPQPosFromProjQN(take, newStartPPQ);
    double takePPQEnd = apis.MIDI_GetPPQPosFromProjQN(take, newEndPPQ);

    bool ok = apis.MIDI_SetNote(take, noteIndex,
                                nullptr, nullptr,          // selected, muted: no change
                                &takePPQStart, &takePPQEnd,
                                nullptr, &newPitch,        // chan: no change
                                nullptr, nullptr);         // vel, noSort: no change

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

bool ReaperMidiWriter::batchInsertNote(int trackIndex, double startPPQ, double endPPQ,
                                       int channel, int pitch, int velocity)
{
    juce::ScopedLock lock(writeLock);
    if (!inBatch || !batchProject) return false;

    void* take = getFirstMidiTake(batchProject, trackIndex);
    if (!take) return false;

    // Disable sorting during batch — we'll sort once at endBatch
    if (batchTake != take)
    {
        if (apis.MIDI_DisableSort)
            apis.MIDI_DisableSort(take);
        batchTake = take;
    }

    double takePPQStart = apis.MIDI_GetPPQPosFromProjQN(take, startPPQ);
    double takePPQEnd = apis.MIDI_GetPPQPosFromProjQN(take, endPPQ);

    const bool noSort = true;
    return apis.MIDI_InsertNote(take, true, false,
                                takePPQStart, takePPQEnd,
                                channel, pitch, velocity, &noSort);
}

bool ReaperMidiWriter::batchDeleteNote(int trackIndex, int noteIndex)
{
    juce::ScopedLock lock(writeLock);
    if (!inBatch || !batchProject) return false;

    void* take = getFirstMidiTake(batchProject, trackIndex);
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
                                     double newStartPPQ, double newEndPPQ, int newPitch)
{
    juce::ScopedLock lock(writeLock);
    if (!inBatch || !batchProject) return false;

    void* take = getFirstMidiTake(batchProject, trackIndex);
    if (!take) return false;

    if (batchTake != take)
    {
        if (apis.MIDI_DisableSort)
            apis.MIDI_DisableSort(take);
        batchTake = take;
    }

    double takePPQStart = apis.MIDI_GetPPQPosFromProjQN(take, newStartPPQ);
    double takePPQEnd = apis.MIDI_GetPPQPosFromProjQN(take, newEndPPQ);

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

    // Sort the take we wrote to
    if (batchTake && apis.MIDI_Sort)
        apis.MIDI_Sort(batchTake);

    endUndoBlock(batchProject, batchDescription.toRawUTF8());

    inBatch = false;
    batchProject = nullptr;
    batchTake = nullptr;
    batchDescription.clear();
}
