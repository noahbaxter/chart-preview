#pragma once

#include <JuceHeader.h>
#include "../Utils/PPQ.h"
#include "../Utils/Utils.h"
#include "../DebugTools/Logger.h"

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

    // Get notes from current project at specified time range
    // trackIndex: 0-based track index (-1 = auto-detect)
    std::vector<ReaperMidiNote> getNotesInRange(double startPPQ, double endPPQ, int trackIndex = -1);

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

    // Get the REAPER API function pointer (for use with ReaperTrackDetector)
    std::function<void*(const char*)> getReaperGetFunc() const {
        if (getReaperApi) {
            return [this](const char* funcname) -> void* { return getReaperApi(funcname); };
        }
        return nullptr;
    }

private:
    // REAPER API function pointers
    void* (*getReaperApi)(const char*) = nullptr;
    bool reaperApiInitialized = false;

    // Core REAPER API functions (dynamically loaded)
    void* (*GetCurrentProject)() = nullptr;
    int (*CountMediaItems)(void* proj) = nullptr;
    void* (*GetMediaItem)(void* proj, int itemidx) = nullptr;
    void* (*GetActiveTake)(void* item) = nullptr;
    void* (*GetMediaItemTake_Track)(void* take) = nullptr;
    double (*GetPlayPosition2Ex)(void* proj) = nullptr;
    double (*GetCursorPositionEx)(void* proj) = nullptr;
    int (*GetPlayState)() = nullptr;

    // MIDI-specific API functions
    int (*MIDI_CountEvts)(void* take, int* notecnt, int* ccevtcnt, int* textsyxevtcnt) = nullptr;
    bool (*MIDI_GetNote)(void* take, int noteidx, bool* selected, bool* muted,
                        double* startppq, double* endppq, int* chan, int* pitch, int* vel) = nullptr;
    double (*MIDI_GetProjQNFromPPQPos)(void* take, double ppqpos) = nullptr;

    // TimeMap API functions for bar/beat calculations
    double (*TimeMap2_QNToTime)(void* proj, double qn) = nullptr;
    double (*TimeMap2_timeToQN)(void* proj, double tpos) = nullptr;
    double (*TimeMap2_timeToBeats)(void* proj, double tpos, int* measuresOut, int* cmlOut,
                                    double* fullbeatsOut, int* cdenomOut) = nullptr;
    double (*TimeMap_GetDividedBpmAtTime)(double time) = nullptr;

    // Helper methods
    bool loadReaperApiFunctions();
    bool isTrackMidiTrack(void* track);
    bool isItemInTimeRange(void* item, double startPPQ, double endPPQ);

    juce::CriticalSection apiLock;
    DebugTools::Logger* logger = nullptr;  // Optional debug logger
};