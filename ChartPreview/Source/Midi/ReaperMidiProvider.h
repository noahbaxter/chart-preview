#pragma once

#include <JuceHeader.h>
#include "../Utils/PPQ.h"
#include "../Utils/Utils.h"

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

    // Set print callback for debug logging
    void setPrintCallback(std::function<void(const juce::String&)> printFunc) { print = printFunc; }

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

    // Helper methods
    bool loadReaperApiFunctions();
    bool isTrackMidiTrack(void* track);
    bool isItemInTimeRange(void* item, double startPPQ, double endPPQ);

    juce::CriticalSection apiLock;
    std::function<void(const juce::String&)> print; // Debug logging callback
};