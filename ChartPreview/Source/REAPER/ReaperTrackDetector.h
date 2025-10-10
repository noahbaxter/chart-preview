/*
  ==============================================================================

    ReaperTrackDetector.h
    Automatic detection of which track contains the Chart Preview plugin

  ==============================================================================
*/

#pragma once

#include <functional>
#include <string>

class ReaperTrackDetector
{
public:
    using ReaperGetFunc = std::function<void*(const char*)>;

    // Detect which track contains the Chart Preview plugin
    // Returns -1 if not found or on error
    static int detectPluginTrack(ReaperGetFunc reaperGetFunc);

    // Get the name of a track by index
    static std::string getTrackName(ReaperGetFunc reaperGetFunc, int trackIndex);

    // Get the total number of tracks in the project
    static int getTrackCount(ReaperGetFunc reaperGetFunc);
};