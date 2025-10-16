/*
  ==============================================================================

    MidiCache.h
    Cache for REAPER timeline MIDI data to prevent redundant fetches

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Utils/PPQ.h"
#include "ReaperMidiProvider.h"

class MidiCache
{
public:
    struct CachedNote
    {
        PPQ startPPQ;
        PPQ endPPQ;
        uint pitch;
        uint velocity;
        uint channel;
        bool muted = false;
        bool processed = false;
    };

    MidiCache() = default;

    // Add notes to the cache (checks for duplicates)
    void addNotes(const std::vector<ReaperMidiProvider::ReaperMidiNote>& notes, PPQ rangeStart, PPQ rangeEnd);

    // Get notes within a specific range
    std::vector<CachedNote> getNotesInRange(PPQ start, PPQ end) const;

    // Remove old notes before a certain position (cleanup)
    void cleanup(PPQ beforePosition);

    // Remove notes outside a specific range (cleanup for REAPER mode)
    void cleanupOutsideRange(PPQ start, PPQ end);

    // Clear all cached data
    void clear();

    // Check if we have data for a given range
    bool hasDataForRange(PPQ start, PPQ end) const;

    // Check if there are any notes in a given range
    bool hasNotesInRange(PPQ start, PPQ end) const;

    // Check if cache is empty
    bool isEmpty() const;

    // Get the current cached range
    PPQ getCacheStartPPQ() const { return cacheStartPPQ; }
    PPQ getCacheEndPPQ() const { return cacheEndPPQ; }

private:
    std::vector<CachedNote> cache;
    mutable juce::CriticalSection cacheLock;

    PPQ cacheStartPPQ{-1.0};
    PPQ cacheEndPPQ{-1.0};
};