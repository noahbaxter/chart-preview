#include "ReaperItemManager.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

ReaperItemManager::ReaperItemManager(const ReaperAPIs& apis,
                                     std::function<void*(const char*)> getFunc)
    : apis(apis), getReaperApi(std::move(getFunc))
{
}

void* ReaperItemManager::getFirstMidiTake(void* project, int trackIndex)
{
    if (!project || !apis.GetTrack || !apis.CountMediaItems ||
        !apis.GetMediaItem || !apis.GetActiveTake || !apis.GetMediaItemTake_Track)
        return nullptr;

    void* track = apis.GetTrack(project, trackIndex);
    if (!track) return nullptr;

    int itemCount = apis.CountMediaItems(project);
    for (int i = 0; i < itemCount; i++)
    {
        void* item = apis.GetMediaItem(project, i);
        if (!item) continue;
        void* take = apis.GetActiveTake(item);
        if (!take) continue;
        if (apis.GetMediaItemTake_Track(take) != track) continue;
        int n = 0;
        if (apis.MIDI_CountEvts && apis.MIDI_CountEvts(take, &n, nullptr, nullptr))
            return take;
    }
    return nullptr;
}

void* ReaperItemManager::getTakeForWrite(void* project, int trackIndex,
                                          double startQN, double endQN)
{
    if (!project || !apis.GetTrack) return nullptr;

    void* track = apis.GetTrack(project, trackIndex);
    if (!track) return nullptr;

    const bool canQuery = apis.CountTrackMediaItems && apis.GetTrackMediaItem &&
                          apis.GetActiveTake && apis.MIDI_CountEvts &&
                          apis.GetMediaItemInfo_Value && apis.TimeMap2_QNToTime;

    if (!canQuery)
        return getFirstMidiTake(project, trackIndex);

    const double rangeStart = apis.TimeMap2_QNToTime(project, startQN);
    const double rangeEnd   = apis.TimeMap2_QNToTime(project, endQN);

    struct ItemInfo { void* item; void* take; double pos; double len; };
    std::vector<ItemInfo> items;
    int count = apis.CountTrackMediaItems(track);
    for (int i = 0; i < count; ++i)
    {
        void* it = apis.GetTrackMediaItem(track, i);
        if (!it) continue;
        void* tk = apis.GetActiveTake(it);
        if (!tk) continue;
        int n = 0;
        if (!apis.MIDI_CountEvts(tk, &n, nullptr, nullptr)) continue;
        double pos = apis.GetMediaItemInfo_Value(it, "D_POSITION");
        double len = apis.GetMediaItemInfo_Value(it, "D_LENGTH");
        items.push_back({ it, tk, pos, len });
    }

    // 1) Containment — existing item already covers the range.
    for (auto& it : items)
        if (it.pos <= rangeStart && it.pos + it.len >= rangeEnd)
            return it.take;

    // 2) Extend the closest item via MIDI_SetItemExtents.
    if (!items.empty() && apis.MIDI_SetItemExtents && apis.MIDI_GetProjQNFromPPQPos)
    {
        ItemInfo* closest = nullptr;
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

            // Snap extension to the previous/next measure boundary (4 QN = 1 bar in 4/4)
            if (startQN < itemStartQN)
                newStartQN = std::max(0.0, std::floor(startQN / 4.0) * 4.0);
            if (endQN > itemEndQN)
                newEndQN = std::ceil(endQN / 4.0) * 4.0;

            // Clamp against neighboring items — never overlap another clip
            for (auto& it : items)
            {
                if (it.item == closest->item) continue;
                double itEnd = it.pos + it.len;
                double itEndQN = apis.TimeMap2_timeToQN(project, itEnd);
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

    // 3) No items — create a 1-bar item snapped to measure floor.
    if (!apis.CreateNewMIDIItemInProj || !apis.TimeMap2_QNToTime)
        return nullptr;

    double barFloor = std::floor(startQN / 4.0) * 4.0;
    double barEnd   = barFloor + 4.0;
    if (endQN > barEnd) barEnd = endQN + 4.0;
    double iStart = apis.TimeMap2_QNToTime(project, barFloor);
    double iEnd   = apis.TimeMap2_QNToTime(project, barEnd);

    void* newItem = apis.CreateNewMIDIItemInProj(track, iStart, iEnd, nullptr);
    if (!newItem) return nullptr;
    if (apis.SetMediaItemInfo_Value)
        apis.SetMediaItemInfo_Value(newItem, "B_LOOPSRC", 0.0);
    return apis.GetActiveTake ? apis.GetActiveTake(newItem) : nullptr;
}
