#pragma once

#include <JuceHeader.h>
#include "ReaperApiHelpers.h"

class ReaperItemManager
{
public:
    ReaperItemManager(const ReaperAPIs& apis, std::function<void*(const char*)> getFunc);

    // Find a MIDI take covering [startQN, endQN] on the given track.
    // If no item covers the range, extends the closest item via MIDI_SetItemExtents.
    // If no items exist, creates a 1-bar item snapped to the measure floor.
    // Returns the take to write into, or nullptr on failure.
    void* getTakeForWrite(void* project, int trackIndex,
                          double startQN, double endQN);

    // Find the first MIDI take on the track (for read-only ops like delete/move).
    void* getFirstMidiTake(void* project, int trackIndex);

private:
    const ReaperAPIs& apis;
    std::function<void*(const char*)> getReaperApi;
};
