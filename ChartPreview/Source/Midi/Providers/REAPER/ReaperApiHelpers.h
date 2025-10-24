/*
  ==============================================================================

    ReaperApiHelpers.h
    Header-only utility for common REAPER API patterns

    Consolidates repeated patterns for getting projects, tracks, and handling
    common REAPER API calls to eliminate duplication across provider classes.

  ==============================================================================
*/

#pragma once

#include <functional>
#include <JuceHeader.h>

/**
 * Comprehensive struct holding all REAPER API functions used across the plugin.
 * This is the single source of truth for all REAPER API function pointers.
 * Individual classes load only what they need, or can use this struct for everything.
 */
struct ReaperAPIs
{
    // Project/timeline functions
    void* (*GetCurrentProject)() = nullptr;

    // Track/item enumeration functions
    void* (*GetTrack)(void*, int) = nullptr;
    int (*CountMediaItems)(void* proj) = nullptr;
    void* (*GetMediaItem)(void* proj, int itemidx) = nullptr;
    void* (*GetActiveTake)(void* item) = nullptr;
    void* (*GetMediaItemTake_Track)(void* take) = nullptr;

    // Playback state functions
    double (*GetPlayPosition2Ex)(void* proj) = nullptr;
    double (*GetCursorPositionEx)(void* proj) = nullptr;
    int (*GetPlayState)() = nullptr;

    // MIDI note functions
    int (*MIDI_CountEvts)(void* take, int* notecnt, int* ccevtcnt, int* textsyxevtcnt) = nullptr;
    bool (*MIDI_GetNote)(void* take, int noteidx, bool* selected, bool* muted,
                        double* startppq, double* endppq, int* chan, int* pitch, int* vel) = nullptr;
    double (*MIDI_GetProjQNFromPPQPos)(void* take, double ppqpos) = nullptr;
    bool (*MIDI_GetTrackHash)(void* track, bool notesonly, char* hashOut, int hashOut_sz) = nullptr;

    // Time mapping functions
    double (*TimeMap2_QNToTime)(void* proj, double qn) = nullptr;
    double (*TimeMap2_timeToQN)(void* proj, double tpos) = nullptr;
    double (*TimeMap2_timeToBeats)(void* proj, double tpos, int* measuresOut, int* cmlOut,
                                   double* fullbeatsOut, int* cdenomOut) = nullptr;
    double (*TimeMap_GetDividedBpmAtTime)(double time) = nullptr;

    // Tempo/time signature marker functions
    int (*CountTempoTimeSigMarkers)(void* proj) = nullptr;
    bool (*GetTempoTimeSigMarker)(void* proj, int idx, double* timepos, int* measurepos,
                                 double* beatpos, double* bpm, int* timesig_num, int* timesig_denom,
                                 bool* lineartempo) = nullptr;

    // Check if all critical APIs for basic operation loaded successfully
    bool isLoaded() const
    {
        return CountMediaItems != nullptr && GetMediaItem != nullptr &&
               GetActiveTake != nullptr && GetPlayPosition2Ex != nullptr &&
               GetCursorPositionEx != nullptr && GetPlayState != nullptr &&
               MIDI_CountEvts != nullptr && MIDI_GetNote != nullptr &&
               MIDI_GetProjQNFromPPQPos != nullptr && TimeMap2_QNToTime != nullptr &&
               TimeMap2_timeToBeats != nullptr;
    }
};

class ReaperApiHelpers
{
public:
    // Get current REAPER project
    // Returns nullptr if EnumProjects is unavailable or fails
    static void* getProject(std::function<void*(const char*)> apiFunc)
    {
        if (!apiFunc)
            return nullptr;

        auto EnumProjects = (void*(*)(int, char*, int))apiFunc("EnumProjects");
        if (!EnumProjects)
            return nullptr;

        return EnumProjects(-1, nullptr, 0);
    }

    // Get track by index from project
    // Returns nullptr if GetTrack is unavailable or track not found
    static void* getTrack(std::function<void*(const char*)> apiFunc,
                         void* project,
                         int trackIndex)
    {
        if (!apiFunc || !project)
            return nullptr;

        auto GetTrack = (void*(*)(void*, int))apiFunc("GetTrack");
        if (!GetTrack)
            return nullptr;

        return GetTrack(project, trackIndex);
    }

    // Get track by index (convenience method that fetches project internally)
    // Returns nullptr if project cannot be fetched or track not found
    static void* getTrack(std::function<void*(const char*)> apiFunc, int trackIndex)
    {
        void* project = getProject(apiFunc);
        if (!project)
            return nullptr;

        return getTrack(apiFunc, project, trackIndex);
    }

    // Load all REAPER API functions from the REAPER extension
    // Returns true if all critical APIs loaded successfully
    static bool loadAPIs(std::function<void*(const char*)> apiFunc, ReaperAPIs& outAPIs)
    {
        if (!apiFunc)
            return false;

        // Project/timeline
        outAPIs.GetCurrentProject = (void*(*)())apiFunc("EnumProjects");

        // Track/item enumeration
        outAPIs.GetTrack = (void*(*)(void*, int))apiFunc("GetTrack");
        outAPIs.CountMediaItems = (int(*)(void*))apiFunc("CountMediaItems");
        outAPIs.GetMediaItem = (void*(*)(void*, int))apiFunc("GetMediaItem");
        outAPIs.GetActiveTake = (void*(*)(void*))apiFunc("GetActiveTake");
        outAPIs.GetMediaItemTake_Track = (void*(*)(void*))apiFunc("GetMediaItemTake_Track");

        // Playback state
        outAPIs.GetPlayPosition2Ex = (double(*)(void*))apiFunc("GetPlayPosition2Ex");
        outAPIs.GetCursorPositionEx = (double(*)(void*))apiFunc("GetCursorPositionEx");
        outAPIs.GetPlayState = (int(*)())apiFunc("GetPlayState");

        // MIDI notes
        outAPIs.MIDI_CountEvts = (int(*)(void*, int*, int*, int*))apiFunc("MIDI_CountEvts");
        outAPIs.MIDI_GetNote = (bool(*)(void*, int, bool*, bool*, double*, double*, int*, int*, int*))apiFunc("MIDI_GetNote");
        outAPIs.MIDI_GetProjQNFromPPQPos = (double(*)(void*, double))apiFunc("MIDI_GetProjQNFromPPQPos");
        outAPIs.MIDI_GetTrackHash = (bool(*)(void*, bool, char*, int))apiFunc("MIDI_GetTrackHash");

        // Time mapping
        outAPIs.TimeMap2_QNToTime = (double(*)(void*, double))apiFunc("TimeMap2_QNToTime");
        outAPIs.TimeMap2_timeToQN = (double(*)(void*, double))apiFunc("TimeMap2_timeToQN");
        outAPIs.TimeMap2_timeToBeats = (double(*)(void*, double, int*, int*, double*, int*))apiFunc("TimeMap2_timeToBeats");
        outAPIs.TimeMap_GetDividedBpmAtTime = (double(*)(double))apiFunc("TimeMap_GetDividedBpmAtTime");

        // Tempo/time signature markers
        outAPIs.CountTempoTimeSigMarkers = (int(*)(void*))apiFunc("CountTempoTimeSigMarkers");
        outAPIs.GetTempoTimeSigMarker = (bool(*)(void*, int, double*, int*, double*, double*, int*, int*, bool*))
            apiFunc("GetTempoTimeSigMarker");

        return outAPIs.isLoaded();
    }

    // Execute a query with automatic locking, initialization check, and project fetching
    // Pattern: lock -> check initialized -> get project -> call query -> return result
    // Template parameters:
    //   Callable: any callable that takes void* project and returns Result
    //   Result: the return type of the query callable
    template<typename Callable, typename Result>
    static Result performQuery(
        std::function<void*(const char*)> apiFunc,
        bool isInitialized,
        juce::CriticalSection& lock,
        Callable&& query,
        Result fallback)
    {
        if (!isInitialized || !apiFunc)
            return fallback;

        juce::ScopedLock scopedLock(lock);
        try
        {
            void* project = getProject(apiFunc);
            if (!project)
                return fallback;

            return query(project);
        }
        catch (...)
        {
            return fallback;
        }
    }
};
