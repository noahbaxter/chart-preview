/*
  ==============================================================================

    ReaperMidiWriter.cpp
    REAPER implementation of MidiWriter

  ==============================================================================
*/

#include "ReaperMidiWriter.h"
#include "ReaperApiHelpers.h"

namespace
{
    // REAPER action IDs
    constexpr int ACTION_ITEM_GLUE          = 40543; // Item: Glue items
    constexpr int ACTION_ITEM_UNSELECT_ALL  = 40289; // Item: Unselect all items
}

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
// Lazy item consolidation
// =============================================================================

bool ReaperMidiWriter::consolidateItemsIfNeeded(void* project, int trackIndex)
{
    if (!project) return false;

    // Need full set of selection + action APIs to safely glue.
    if (!apis.GetTrack || !apis.CountTrackMediaItems || !apis.GetTrackMediaItem ||
        !apis.Main_OnCommand || !apis.SetMediaItemSelected || !apis.IsMediaItemSelected ||
        !apis.CountSelectedMediaItems || !apis.GetSelectedMediaItem)
    {
        // No consolidation infrastructure — just proceed; the writer will
        // target the first MIDI item.
        return true;
    }

    void* track = apis.GetTrack(project, trackIndex);
    if (!track) return false;

    int trackItemCount = apis.CountTrackMediaItems(track);
    if (trackItemCount <= 1)
        return true; // nothing to consolidate

    juce::Logger::writeToLog("ReaperMidiWriter: consolidating "
                             + juce::String(trackItemCount)
                             + " items on track " + juce::String(trackIndex));

    // Save current item selection so we can restore it.
    int prevSelCount = apis.CountSelectedMediaItems(project);
    std::vector<void*> prevSelection;
    prevSelection.reserve((size_t)prevSelCount);
    for (int i = 0; i < prevSelCount; ++i)
    {
        if (void* it = apis.GetSelectedMediaItem(project, i))
            prevSelection.push_back(it);
    }

    // Unselect everything, then select only this track's items.
    apis.Main_OnCommand(ACTION_ITEM_UNSELECT_ALL, 0);
    for (int i = 0; i < trackItemCount; ++i)
    {
        if (void* it = apis.GetTrackMediaItem(track, i))
            apis.SetMediaItemSelected(it, true);
    }

    // Glue them. Wrap in its own undo step.
    juce::String desc = "Chartchotic: Consolidate items on track " + juce::String(trackIndex);
    apis.Main_OnCommand(ACTION_ITEM_GLUE, 0);
    if (apis.MarkProjectDirty)
        apis.MarkProjectDirty(project);
    if (apis.Undo_OnStateChange)
        apis.Undo_OnStateChange(desc.toRawUTF8());

    // Restore prior selection (best-effort — glued items have new pointers,
    // so previously-selected items on this track simply won't reappear).
    apis.Main_OnCommand(ACTION_ITEM_UNSELECT_ALL, 0);
    for (void* it : prevSelection)
    {
        if (it) apis.SetMediaItemSelected(it, true);
    }

    return true;
}

// =============================================================================
// Undo helpers
// =============================================================================

void ReaperMidiWriter::beginUndoBlock(void* /*project*/, const char* /*description*/)
{
    // BeginBlock2/EndBlock2 don't work from plugin GUI threads — the block
    // never properly closes, causing all subsequent operations to be swallowed
    // into a single never-ending undo block. Intentionally empty.
}

void ReaperMidiWriter::endUndoBlock(void* project, const char* description)
{
    if (apis.MarkProjectDirty)
        apis.MarkProjectDirty(project);

    // Undo_OnStateChange is the only undo call that reliably works from
    // plugin GUI threads. Creates a standalone undo point per operation.
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

    consolidateItemsIfNeeded(project, trackIndex);

    void* take = getFirstMidiTake(project, trackIndex);
    if (!take) return false;

    beginUndoBlock(project, "Chartchotic: Insert note");

    // Convert project QN to take PPQ
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

    consolidateItemsIfNeeded(project, trackIndex);

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
                                double newStartQN, double newEndQN, int newPitch)
{
    juce::ScopedLock lock(writeLock);

    void* project = ReaperApiHelpers::getProject(getReaperApi);
    if (!project) return false;

    consolidateItemsIfNeeded(project, trackIndex);

    void* take = getFirstMidiTake(project, trackIndex);
    if (!take) return false;

    beginUndoBlock(project, "Chartchotic: Move note");

    // Convert project QN to take PPQ
    double takePPQStart = apis.MIDI_GetPPQPosFromProjQN(take, newStartQN);
    double takePPQEnd = apis.MIDI_GetPPQPosFromProjQN(take, newEndQN);

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

bool ReaperMidiWriter::batchInsertNote(int trackIndex, double startQN, double endQN,
                                       int channel, int pitch, int velocity)
{
    juce::ScopedLock lock(writeLock);
    if (!inBatch || !batchProject) return false;

    consolidateItemsIfNeeded(batchProject, trackIndex);

    void* take = getFirstMidiTake(batchProject, trackIndex);
    if (!take) return false;

    // Disable sorting during batch — we'll sort once at endBatch
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

    consolidateItemsIfNeeded(batchProject, trackIndex);

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
                                     double newStartQN, double newEndQN, int newPitch)
{
    juce::ScopedLock lock(writeLock);
    if (!inBatch || !batchProject) return false;

    consolidateItemsIfNeeded(batchProject, trackIndex);

    void* take = getFirstMidiTake(batchProject, trackIndex);
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

    // Sort the take we wrote to
    if (batchTake && apis.MIDI_Sort)
        apis.MIDI_Sort(batchTake);

    endUndoBlock(batchProject, batchDescription.toRawUTF8());

    inBatch = false;
    batchProject = nullptr;
    batchTake = nullptr;
    batchDescription.clear();
}
