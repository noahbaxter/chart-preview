/*
  ==============================================================================

    ReaperNoteFetcher.h
    Handles track iteration and MIDI note extraction from REAPER

    Consolidates all track/item/note enumeration logic in one place
    to eliminate duplication between windowed and bulk fetch methods.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "ReaperMidiProvider.h"
#include "ReaperApiHelpers.h"
#include "../../../Utils/PPQ.h"
#include "../../../DebugTools/Logger.h"

/**
 * Handles all track-level MIDI data extraction from REAPER.
 *
 * Responsibilities:
 * - Detect and validate track
 * - Iterate through media items
 * - Extract MIDI notes with PPQ conversion
 * - Support both windowed and bulk fetches
 */
class ReaperNoteFetcher
{
public:
    ReaperNoteFetcher(std::function<void*(const char*)> reaperApiFunc, const ReaperAPIs& apis);
    ~ReaperNoteFetcher();

    // Set debug logger
    void setLogger(DebugTools::Logger* loggerPtr) { logger = loggerPtr; }

    // Fetch ALL notes from a track (bulk operation)
    std::vector<ReaperMidiProvider::ReaperMidiNote> fetchAllNotes(int trackIndex = -1);

    // Fetch notes within a specific PPQ range (windowed operation)
    std::vector<ReaperMidiProvider::ReaperMidiNote> fetchNotesInRange(double startPPQ, double endPPQ, int trackIndex = -1);

private:
    std::function<void*(const char*)> getReaperApi;  // For track detection only
    const ReaperAPIs& apis;
    DebugTools::Logger* logger = nullptr;

    // Helper: Get the target track with auto-detection
    void* getTargetTrack(void* project, int& trackIndex);

    // Helper: Extract notes from a single MIDI take
    void extractNotesFromTake(void* take,
                             std::vector<ReaperMidiProvider::ReaperMidiNote>& outNotes,
                             double startPPQ = -std::numeric_limits<double>::infinity(),
                             double endPPQ = std::numeric_limits<double>::infinity());

    // Core: Iterate media items and extract notes
    std::vector<ReaperMidiProvider::ReaperMidiNote> iterateAndExtractNotes(void* project,
                                                                             void* targetTrack,
                                                                             double startPPQ,
                                                                             double endPPQ,
                                                                             bool filterByRange);

    juce::CriticalSection apiLock;
};
