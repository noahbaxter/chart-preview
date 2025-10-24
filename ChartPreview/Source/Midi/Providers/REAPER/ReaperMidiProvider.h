#pragma once

#include <JuceHeader.h>
#include "ReaperApiHelpers.h"
#include "../../../Utils/PPQ.h"
#include "../../../Utils/Utils.h"
#include "../../../Utils/TimeConverter.h"
#include "../../../DebugTools/Logger.h"

class ReaperNoteFetcher;  // Forward declaration

/**
 * Provides MIDI note data directly from REAPER's timeline for scrubbing and lookahead.
 * Bypasses VST audio buffer limitations by reading from REAPER's project data.
 */
class ReaperMidiProvider
{
public:
    ReaperMidiProvider();
    ~ReaperMidiProvider();

    // Initialize with REAPER API function getter (from VST2 integration)
    // Returns true if initialization succeeded
    bool initialize(void* (*reaperGetApiFunc)(const char*));

    // Set debug logger
    void setLogger(DebugTools::Logger* loggerPtr) { logger = loggerPtr; }

    // Check if REAPER API is available and working
    bool isReaperApiAvailable() const { return reaperApiInitialized; }

    // Get MIDI notes for a time range from REAPER's timeline
    struct ReaperMidiNote {
        double startPPQ;
        double endPPQ;
        int channel;
        int pitch;
        int velocity;
        bool selected;
        bool muted;
    };

    // Get notes from a specific time range (DEPRECATED - for backward compatibility)
    // Use getAllNotesFromTrack() for new code instead
    std::vector<ReaperMidiNote> getNotesInRange(double startPPQ, double endPPQ, int trackIndex = -1);

    // Get ALL notes from a track in the entire session (delegates to ReaperNoteFetcher)
    // trackIndex: 0-based track index (-1 = auto-detect)
    std::vector<ReaperMidiNote> getAllNotesFromTrack(int trackIndex = -1);

    // Get ALL tempo and time signature events in the entire session
    // Returns events sorted by PPQ position
    std::vector<TempoTimeSignatureEvent> getAllTempoTimeSignatureEvents();

    // Get hash of MIDI data on track - changes only when MIDI changes
    // Useful for efficient polling to detect MIDI edits
    // notesonly: if true, only changes when notes change (ignores CC changes)
    std::string getTrackHash(int trackIndex, bool notesonly = true);

    // Get current playback/cursor positions
    double getCurrentPlayPosition();
    double getCurrentCursorPosition();

    // Check if transport is playing
    bool isPlaying();

    // Time signature and musical position queries
    struct MusicalPosition {
        int measure;              // Bar/measure number (0-indexed)
        double beatInMeasure;     // Beat position within the measure (0.0 to timesig_num)
        double fullBeats;         // Total beats from song start
        int timesig_num;          // Time signature numerator (e.g., 3 for 3/4)
        int timesig_denom;        // Time signature denominator (e.g., 4 for 3/4)
        double bpm;               // Current tempo
    };

    // Get musical position (bar/beat) at a given PPQ position
    MusicalPosition getMusicalPositionAtPPQ(double ppq);

    // Convert PPQ (quarter note position) to absolute time in seconds
    // This uses REAPER's timeline which handles ALL tempo changes correctly
    double ppqToTime(double ppq);

    // Get the REAPER API function pointer (for use with ReaperTrackDetector)
    std::function<void*(const char*)> getReaperGetFunc() const {
        if (getReaperApi) {
            return [this](const char* funcname) -> void* { return getReaperApi(funcname); };
        }
        return nullptr;
    }

private:
    std::unique_ptr<ReaperNoteFetcher> noteFetcher;

    // REAPER API function pointers
    void* (*getReaperApi)(const char*) = nullptr;
    bool reaperApiInitialized = false;

    // All REAPER API functions consolidated in one struct
    ReaperAPIs apis;

    // Helper methods

    // Extract and process all tempo/timesig markers into events vector
    void processTempoMarkers(void* project, std::vector<TempoTimeSignatureEvent>& events);

    // Query REAPER for musical position at a given time
    MusicalPosition queryMusicalPositionFromReaper(void* project, double ppq, double timeInSeconds) const;

    juce::CriticalSection apiLock;
    DebugTools::Logger* logger = nullptr;  // Optional debug logger
};