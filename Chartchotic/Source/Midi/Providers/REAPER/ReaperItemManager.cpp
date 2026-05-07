#include "ReaperItemManager.h"

#include <algorithm>
#include <cmath>
#include <limits>

ReaperItemManager::ReaperItemManager(const ReaperAPIs& apis,
                                     std::function<void*(const char*)> getFunc)
    : apis(apis), getReaperApi(std::move(getFunc))
{
}

std::vector<ReaperItemManager::MidiItemInfo>
ReaperItemManager::collectMidiItems(void* project, int trackIndex)
{
    std::vector<MidiItemInfo> items;
    if (!project || !apis.GetTrack || !apis.CountTrackMediaItems ||
        !apis.GetTrackMediaItem || !apis.GetActiveTake ||
        !apis.MIDI_CountEvts || !apis.GetMediaItemInfo_Value)
        return items;

    void* track = apis.GetTrack(project, trackIndex);
    if (!track) return items;

    int count = apis.CountTrackMediaItems(track);
    for (int i = 0; i < count; ++i)
    {
        void* item = apis.GetTrackMediaItem(track, i);
        if (!item) continue;
        void* take = apis.GetActiveTake(item);
        if (!take) continue;
        int n = 0;
        if (!apis.MIDI_CountEvts(take, &n, nullptr, nullptr)) continue;
        double pos = apis.GetMediaItemInfo_Value(item, "D_POSITION");
        double len = apis.GetMediaItemInfo_Value(item, "D_LENGTH");
        items.push_back({ item, take, pos, len });
    }
    return items;
}

// =============================================================================
// getTakeForWrite — position lookup with extend/create fallback
// =============================================================================

void* ReaperItemManager::getTakeForWrite(void* project, int trackIndex,
                                          double startQN, double endQN)
{
    if (!project || !apis.GetTrack || !apis.TimeMap2_QNToTime) return nullptr;

    auto items = collectMidiItems(project, trackIndex);
    const double rangeStart = apis.TimeMap2_QNToTime(project, startQN);
    const double rangeEnd   = apis.TimeMap2_QNToTime(project, endQN);

    for (auto& it : items)
        if (it.pos <= rangeStart && it.pos + it.len >= rangeEnd)
            return it.take;

    // Extend closest item
    if (!items.empty() && apis.MIDI_SetItemExtents && apis.MIDI_GetProjQNFromPPQPos &&
        apis.TimeMap2_timeToQN)
    {
        MidiItemInfo* closest = nullptr;
        double bestDist = std::numeric_limits<double>::max();
        double midRange = (rangeStart + rangeEnd) * 0.5;
        for (auto& it : items)
        {
            double itEnd = it.pos + it.len;
            double dist = (midRange < it.pos) ? (it.pos - midRange)
                        : (midRange > itEnd)  ? (midRange - itEnd)
                                              : 0.0;
            if (dist < bestDist) { bestDist = dist; closest = &it; }
        }

        if (closest)
        {
            double itemStartQN = apis.TimeMap2_timeToQN(project, closest->pos);
            double itemEndQN   = apis.TimeMap2_timeToQN(project, closest->pos + closest->len);
            double newStartQN = itemStartQN;
            double newEndQN   = itemEndQN;

            if (startQN < itemStartQN)
                newStartQN = std::max(0.0, std::floor(startQN / 4.0) * 4.0);
            if (endQN > itemEndQN)
                newEndQN = std::ceil(endQN / 4.0) * 4.0;

            for (auto& it : items)
            {
                if (it.item == closest->item) continue;
                double itEndQN   = apis.TimeMap2_timeToQN(project, it.pos + it.len);
                double itStartQN = apis.TimeMap2_timeToQN(project, it.pos);
                if (itEndQN <= itemStartQN && itEndQN > newStartQN)
                    newStartQN = itEndQN;
                if (itStartQN >= itemEndQN && itStartQN < newEndQN)
                    newEndQN = itStartQN;
            }

            apis.MIDI_SetItemExtents(closest->item, newStartQN, newEndQN);
            return closest->take;
        }
    }

    // Create new item
    void* track = apis.GetTrack(project, trackIndex);
    if (!track || !apis.CreateNewMIDIItemInProj) return nullptr;

    double barFloor = std::floor(startQN / 4.0) * 4.0;
    double barEnd   = barFloor + 4.0;
    if (endQN > barEnd) barEnd = endQN + 4.0;

    void* newItem = apis.CreateNewMIDIItemInProj(track,
        apis.TimeMap2_QNToTime(project, barFloor),
        apis.TimeMap2_QNToTime(project, barEnd), nullptr);
    if (!newItem) return nullptr;
    if (apis.SetMediaItemInfo_Value)
        apis.SetMediaItemInfo_Value(newItem, "B_LOOPSRC", 0.0);
    return apis.GetActiveTake ? apis.GetActiveTake(newItem) : nullptr;
}

// =============================================================================
// findNoteIndex — searches ALL takes, caches the result take
// =============================================================================

int ReaperItemManager::findNoteIndex(void* project, int trackIndex,
                                      double targetQN, int pitch, double toleranceQN)
{
    lastFoundTake = nullptr;
    lastFoundIdx  = -1;
    if (!apis.MIDI_CountEvts || !apis.MIDI_GetNote || !apis.MIDI_GetPPQPosFromProjQN)
        return -1;

    auto items = collectMidiItems(project, trackIndex);

    int bestIdx = -1;
    double bestDist = std::numeric_limits<double>::max();

    for (auto& it : items)
    {
        double targetPPQ = apis.MIDI_GetPPQPosFromProjQN(it.take, targetQN);
        double tolPPQ = apis.MIDI_GetPPQPosFromProjQN(it.take, targetQN + toleranceQN) - targetPPQ;

        int noteCount = 0;
        apis.MIDI_CountEvts(it.take, &noteCount, nullptr, nullptr);

        for (int i = 0; i < noteCount; ++i)
        {
            double startPPQ = 0, endPPQ = 0;
            int ch = 0, p = 0, vel = 0;
            bool sel = false, muted = false;
            if (!apis.MIDI_GetNote(it.take, i, &sel, &muted, &startPPQ, &endPPQ, &ch, &p, &vel))
                continue;
            if (p != pitch) continue;

            // Match near note start OR inside sustain body [startPPQ, endPPQ]
            bool insideBody = (targetPPQ >= startPPQ && targetPPQ < endPPQ);
            double dist = insideBody ? 0.0 : std::abs(startPPQ - targetPPQ);
            if ((insideBody || dist < tolPPQ) && dist < bestDist)
            {
                bestDist = dist;
                bestIdx = i;
                lastFoundTake = it.take;
                lastFoundIdx  = i;
            }
        }
    }

    return bestIdx;
}

// =============================================================================
// findNotesInRange — returns all notes matching pitch within a QN range
// =============================================================================

std::vector<MidiWriter::NoteInfo>
ReaperItemManager::findNotesInRange(void* project, int trackIndex,
                                     double startQN, double endQN, int pitch)
{
    std::vector<MidiWriter::NoteInfo> results;
    if (!apis.MIDI_CountEvts || !apis.MIDI_GetNote || !apis.MIDI_GetPPQPosFromProjQN ||
        !apis.MIDI_GetProjQNFromPPQPos)
        return results;

    auto items = collectMidiItems(project, trackIndex);

    for (auto& it : items)
    {
        double startPPQ = apis.MIDI_GetPPQPosFromProjQN(it.take, startQN);
        double endPPQ   = apis.MIDI_GetPPQPosFromProjQN(it.take, endQN);

        int noteCount = 0;
        apis.MIDI_CountEvts(it.take, &noteCount, nullptr, nullptr);

        for (int i = 0; i < noteCount; ++i)
        {
            double nStart = 0, nEnd = 0;
            int ch = 0, p = 0, vel = 0;
            bool sel = false, muted = false;
            if (!apis.MIDI_GetNote(it.take, i, &sel, &muted, &nStart, &nEnd, &ch, &p, &vel))
                continue;
            if (p != pitch) continue;
            if (nStart < startPPQ || nStart > endPPQ) continue;

            double noteStartQN = apis.MIDI_GetProjQNFromPPQPos(it.take, nStart);
            double noteEndQN   = apis.MIDI_GetProjQNFromPPQPos(it.take, nEnd);
            results.push_back({ i, noteStartQN, noteEndQN, p });
        }
    }

    std::sort(results.begin(), results.end(),
              [](const auto& a, const auto& b) { return a.startQN < b.startQN; });
    return results;
}
