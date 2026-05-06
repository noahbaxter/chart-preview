/*
  ==============================================================================

    ReaperMidiWriter.cpp
    REAPER implementation of MidiWriter

  ==============================================================================
*/

#include "ReaperMidiWriter.h"
#include "ReaperApiHelpers.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

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
    return getFirstMidiTakeAndItem(project, trackIndex, nullptr);
}

void* ReaperMidiWriter::getFirstMidiTakeAndItem(void* project, int trackIndex, void** outItem)
{
    if (outItem) *outItem = nullptr;

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
            {
                if (outItem) *outItem = item;
                return take;
            }
        }
    }

    return nullptr;
}

// =============================================================================
// Item-bounds management
//
// Ensures a MIDI item on the target track covers [startQN, endQN]. Walks all
// MIDI items on the track (not just index 0) and picks the best target by
// containment > overlap > closest-extendable, clamping any extension against
// neighboring items so we never overrun a sibling. If no extendable item
// exists, creates a new item with bounds trimmed to fit the available gap.
// =============================================================================

namespace
{
    struct ItemInfo
    {
        void*  item;
        void*  take;
        double pos;
        double len;
    };
}

void* ReaperMidiWriter::ensureItemCoversRange(void* project, int trackIndex,
                                              double startQN, double endQN)
{
    if (!project) return nullptr;

    const bool canResize  = apis.GetMediaItemInfo_Value != nullptr &&
                            apis.SetMediaItemInfo_Value != nullptr &&
                            apis.TimeMap2_QNToTime      != nullptr;
    const bool canCreate  = apis.CreateNewMIDIItemInProj != nullptr &&
                            apis.TimeMap2_QNToTime       != nullptr &&
                            apis.GetTrack                != nullptr;
    const bool canEnumPerTrack = apis.GetTrack != nullptr &&
                                 apis.CountTrackMediaItems != nullptr &&
                                 apis.GetTrackMediaItem != nullptr &&
                                 apis.GetActiveTake != nullptr &&
                                 apis.MIDI_CountEvts != nullptr;

    // Fast path: if the cached item from a previous call still covers the
    // range, skip the full per-track enumeration + vector allocation entirely.
    if (canResize && cachedItem.trackIndex == trackIndex && cachedItem.item != nullptr)
    {
        const double rangeStartCached = apis.TimeMap2_QNToTime(project, startQN);
        const double rangeEndCached   = apis.TimeMap2_QNToTime(project, endQN);
        if (cachedItem.pos <= rangeStartCached && cachedItem.pos + cachedItem.len >= rangeEndCached)
            return cachedItem.take;
    }

    // Older REAPER without per-track enumeration / resize: fall back to the
    // legacy first-take behavior. Don't try to be clever.
    if (!canEnumPerTrack || !canResize)
    {
        void* take = getFirstMidiTake(project, trackIndex);
        if (take) return take;

        if (!canCreate) return nullptr;
        void* track = apis.GetTrack(project, trackIndex);
        if (!track) return nullptr;
        const double sSec = apis.TimeMap2_QNToTime(project, startQN);
        const double eSec = apis.TimeMap2_QNToTime(project, endQN);
        const double iStart = std::max(0.0, sSec - 0.5);
        const double iEnd   = eSec + 0.5;
        void* newItem = apis.CreateNewMIDIItemInProj(track, iStart, iEnd, nullptr);
        if (!newItem) return nullptr;
        disableSourceLoop(newItem);
        return apis.GetActiveTake ? apis.GetActiveTake(newItem) : nullptr;
    }

    void* track = apis.GetTrack(project, trackIndex);
    if (!track) return nullptr;

    const double rangeStart = apis.TimeMap2_QNToTime(project, startQN);
    const double rangeEnd   = apis.TimeMap2_QNToTime(project, endQN);

    // Collect all MIDI items on this track.
    std::vector<ItemInfo> items;
    const int trackItemCount = apis.CountTrackMediaItems(track);
    items.reserve((size_t) std::max(0, trackItemCount));
    for (int i = 0; i < trackItemCount; ++i)
    {
        void* it = apis.GetTrackMediaItem(track, i);
        if (!it) continue;
        void* tk = apis.GetActiveTake(it);
        if (!tk) continue;
        int notes = 0;
        if (!apis.MIDI_CountEvts(tk, &notes, nullptr, nullptr)) continue;
        const double pos = apis.GetMediaItemInfo_Value(it, "D_POSITION");
        const double len = apis.GetMediaItemInfo_Value(it, "D_LENGTH");
        items.push_back({ it, tk, pos, len });
    }

    // 1) Strict containment — already covers the range, no resize needed.
    //    No disableSourceLoop here — bounds didn't change, no loop risk.
    for (const auto& it : items)
    {
        if (it.pos <= rangeStart && it.pos + it.len >= rangeEnd)
        {
            cachedItem = { trackIndex, it.item, it.take, it.pos, it.len };
            return it.take;
        }
    }

    // 2) Overlap — extend the overlapping item, clamped against neighbors.
    ItemInfo* overlap = nullptr;
    for (auto& it : items)
    {
        if (it.pos < rangeEnd && it.pos + it.len > rangeStart)
        {
            overlap = &it;
            break;
        }
    }

    if (overlap)
    {
        double newPos = std::min(overlap->pos, rangeStart);
        double newEnd = std::max(overlap->pos + overlap->len, rangeEnd);

        for (const auto& it : items)
        {
            if (it.item == overlap->item) continue;
            const double oEnd = it.pos + it.len;
            if (oEnd <= overlap->pos)
                newPos = std::max(newPos, oEnd);                  // left neighbor
            if (it.pos >= overlap->pos + overlap->len)
                newEnd = std::min(newEnd, it.pos);                // right neighbor
        }

        const double finalLen = newEnd - newPos;
        resizeItemKeepingNoteTimes(overlap->item, overlap->take,
                                   overlap->pos, newPos,
                                   overlap->len, finalLen);
        disableSourceLoop(overlap->item);
        cachedItem = { trackIndex, overlap->item, overlap->take, newPos, finalLen };
        return overlap->take;
    }

    // 3) No overlap — find the closest item we can extend to cover the range
    //    without crossing any other item.
    double bestDist = std::numeric_limits<double>::max();
    ItemInfo* best = nullptr;
    for (auto& it : items)
    {
        const double itEnd = it.pos + it.len;
        const double dist = (rangeStart > itEnd) ? rangeStart - itEnd
                          : (rangeEnd   < it.pos) ? it.pos - rangeEnd
                                                  : 0.0;
        if (dist >= bestDist) continue;

        const double newPos = std::min(it.pos, rangeStart);
        const double newEnd = std::max(itEnd, rangeEnd);
        bool wouldCross = false;
        for (const auto& other : items)
        {
            if (other.item == it.item) continue;
            const double oEnd = other.pos + other.len;
            if (newPos < oEnd && newEnd > other.pos) { wouldCross = true; break; }
        }
        if (wouldCross) continue;

        bestDist = dist;
        best = &it;
    }

    if (best)
    {
        const double newPos = std::min(best->pos, rangeStart);
        const double newEnd = std::max(best->pos + best->len, rangeEnd);
        const double newLen = newEnd - newPos;
        resizeItemKeepingNoteTimes(best->item, best->take,
                                   best->pos, newPos,
                                   best->len, newLen);
        disableSourceLoop(best->item);
        cachedItem = { trackIndex, best->item, best->take, newPos, newLen };
        return best->take;
    }

    // 4) No extendable item — create a new one, trimmed against neighbors.
    if (!canCreate) return nullptr;

    auto trimAgainstNeighbors = [&items, rangeStart, rangeEnd](double& s, double& e)
    {
        for (const auto& it : items)
        {
            const double oEnd = it.pos + it.len;
            if (oEnd <= rangeStart) s = std::max(s, oEnd);     // left neighbor
            if (it.pos >= rangeEnd) e = std::min(e, it.pos);   // right neighbor
        }
    };

    double itemStart = std::max(0.0, rangeStart - 0.5);
    double itemEnd   = rangeEnd + 0.5;
    trimAgainstNeighbors(itemStart, itemEnd);

    // If the gap is too tight, shrink the headroom and retry once.
    if (itemEnd - itemStart < 0.05)
    {
        itemStart = std::max(0.0, rangeStart - 0.025);
        itemEnd   = rangeEnd + 0.025;
        trimAgainstNeighbors(itemStart, itemEnd);
        if (itemEnd - itemStart < 1e-3) return nullptr;
    }

    void* newItem = apis.CreateNewMIDIItemInProj(track, itemStart, itemEnd, nullptr);
    if (!newItem) return nullptr;

    disableSourceLoop(newItem);

    void* newTake = apis.GetActiveTake(newItem);
    if (newTake)
        cachedItem = { trackIndex, newItem, newTake, itemStart, itemEnd - itemStart };
    return newTake;
}

void ReaperMidiWriter::resizeItemKeepingNoteTimes(void* item, void* take,
                                                  double oldPos, double newPos,
                                                  double oldLen, double newLen)
{
    if (!item || !apis.SetMediaItemInfo_Value) return;

    const bool posChanged = std::abs(newPos - oldPos) > 1e-9;
    const bool lenChanged = std::abs(newLen - oldLen) > 1e-9;

    // Pure length change — no notes shift, just resize.
    if (!posChanged)
    {
        if (lenChanged)
            apis.SetMediaItemInfo_Value(item, "D_LENGTH", newLen);
        return;
    }

    // Position is changing. Capture take PPQ at an arbitrary reference project
    // QN before/after the move; the delta is what every existing note must be
    // shifted by to stay anchored at its original project time.
    const bool canShift = take != nullptr &&
                          apis.MIDI_GetPPQPosFromProjQN != nullptr &&
                          apis.MIDI_CountEvts != nullptr &&
                          apis.MIDI_GetNote != nullptr &&
                          apis.MIDI_SetNote != nullptr;

    const double refProjQN = 0.0;
    const double ppqBefore = canShift ? apis.MIDI_GetPPQPosFromProjQN(take, refProjQN) : 0.0;

    apis.SetMediaItemInfo_Value(item, "D_POSITION", newPos);
    if (lenChanged)
        apis.SetMediaItemInfo_Value(item, "D_LENGTH", newLen);

    if (!canShift) return;

    const double ppqAfter = apis.MIDI_GetPPQPosFromProjQN(take, refProjQN);
    const double ppqShift = ppqAfter - ppqBefore;
    if (std::abs(ppqShift) <= 1e-9) return;

    int noteCount = 0;
    apis.MIDI_CountEvts(take, &noteCount, nullptr, nullptr);
    if (noteCount <= 0) return;

    if (apis.MIDI_DisableSort) apis.MIDI_DisableSort(take);

    for (int i = 0; i < noteCount; ++i)
    {
        bool selected = false, muted = false;
        double startPPQ = 0.0, endPPQ = 0.0;
        int channel = 0, pitch = 0, velocity = 0;
        if (!apis.MIDI_GetNote(take, i, &selected, &muted,
                               &startPPQ, &endPPQ,
                               &channel, &pitch, &velocity))
            continue;

        double newStart = startPPQ + ppqShift;
        double newEnd   = endPPQ   + ppqShift;
        const bool noSort = true;
        apis.MIDI_SetNote(take, i, nullptr, nullptr,
                          &newStart, &newEnd,
                          nullptr, nullptr, nullptr, &noSort);
    }

    if (apis.MIDI_Sort) apis.MIDI_Sort(take);
}

// Disable REAPER's "Loop source" on the item so extending D_LENGTH past the
// MIDI source's natural end leaves blank space instead of replicating content.
// No-op if SetMediaItemInfo_Value isn't available.
void ReaperMidiWriter::disableSourceLoop(void* item)
{
    if (!item || !apis.SetMediaItemInfo_Value) return;
    apis.SetMediaItemInfo_Value(item, "B_LOOPSRC", 0.0);
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

    // Auto-extend item bounds (or auto-create on empty tracks) so writes past
    // the current item end don't get silently truncated by REAPER.
    void* take = ensureItemCoversRange(project, trackIndex, startQN, endQN);
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

    // Auto-extend / auto-create item bounds for out-of-range writes.
    void* take = ensureItemCoversRange(batchProject, trackIndex, startQN, endQN);
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
