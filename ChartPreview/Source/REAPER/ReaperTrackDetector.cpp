/*
  ==============================================================================

    ReaperTrackDetector.cpp
    Automatic detection of which track contains the Chart Preview plugin

  ==============================================================================
*/

#include "ReaperTrackDetector.h"
#include <cstring>

int ReaperTrackDetector::detectPluginTrack(ReaperGetFunc reaperGetFunc)
{
    if (!reaperGetFunc)
        return -1;

    // Get REAPER API functions we need
    auto CountTracks = (int(*)(int))reaperGetFunc("CountTracks");
    auto GetTrack = (void*(*)(int, int))reaperGetFunc("GetTrack");
    auto TrackFX_GetCount = (int(*)(void*))reaperGetFunc("TrackFX_GetCount");
    auto TrackFX_GetFXName = (bool(*)(void*, int, char*, int))reaperGetFunc("TrackFX_GetFXName");

    if (!CountTracks || !GetTrack || !TrackFX_GetCount || !TrackFX_GetFXName)
        return -1;

    int trackCount = CountTracks(0);  // 0 = current project

    // Search all tracks for the Chart Preview plugin
    for (int trackIdx = 0; trackIdx < trackCount; trackIdx++)
    {
        void* track = GetTrack(0, trackIdx);  // 0 = current project
        if (!track) continue;

        int fxCount = TrackFX_GetCount(track);

        // Search all FX on this track
        for (int fxIdx = 0; fxIdx < fxCount; fxIdx++)
        {
            char fxName[256] = {0};
            if (TrackFX_GetFXName(track, fxIdx, fxName, sizeof(fxName)))
            {
                // Check if this is our plugin (case-insensitive)
                if (strstr(fxName, "Chart Preview") != nullptr ||
                    strstr(fxName, "ChartPreview") != nullptr)
                {
                    return trackIdx;  // Found it!
                }
            }
        }
    }

    return -1;  // Not found
}

std::string ReaperTrackDetector::getTrackName(ReaperGetFunc reaperGetFunc, int trackIndex)
{
    if (!reaperGetFunc)
        return "";

    auto GetTrack = (void*(*)(int, int))reaperGetFunc("GetTrack");
    auto GetTrackName = (bool(*)(void*, char*, int))reaperGetFunc("GetTrackName");

    if (!GetTrack || !GetTrackName)
        return "";

    void* track = GetTrack(0, trackIndex);  // 0 = current project
    if (!track)
        return "";

    char trackName[256] = {0};
    if (GetTrackName(track, trackName, sizeof(trackName)))
    {
        return std::string(trackName);
    }

    return "Track " + std::to_string(trackIndex + 1);  // Default name
}

int ReaperTrackDetector::getTrackCount(ReaperGetFunc reaperGetFunc)
{
    if (!reaperGetFunc)
        return 0;

    auto CountTracks = (int(*)(int))reaperGetFunc("CountTracks");
    if (!CountTracks)
        return 0;

    return CountTracks(0);  // 0 = current project
}