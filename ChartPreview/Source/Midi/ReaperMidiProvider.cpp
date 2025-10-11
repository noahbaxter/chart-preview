#include "ReaperMidiProvider.h"
#include "../REAPER/ReaperTrackDetector.h"

ReaperMidiProvider::ReaperMidiProvider()
{
}

ReaperMidiProvider::~ReaperMidiProvider()
{
}

bool ReaperMidiProvider::initialize(void* (*reaperGetApiFunc)(const char*))
{
    juce::ScopedLock lock(apiLock);

    getReaperApi = reaperGetApiFunc;

    if (!getReaperApi)
    {
        reaperApiInitialized = false;
        return false;
    }

    // Add safety check to prevent crashes with invalid function pointers
    try
    {
        // Load all required REAPER API functions
        reaperApiInitialized = loadReaperApiFunctions();
        return reaperApiInitialized;
    }
    catch (...)
    {
        reaperApiInitialized = false;
        getReaperApi = nullptr;
        return false;
    }
}

bool ReaperMidiProvider::loadReaperApiFunctions()
{
    // Load core project functions
    GetCurrentProject = (void*(*)())getReaperApi("EnumProjects");
    CountMediaItems = (int(*)(void*))getReaperApi("CountMediaItems");
    GetMediaItem = (void*(*)(void*, int))getReaperApi("GetMediaItem");
    GetActiveTake = (void*(*)(void*))getReaperApi("GetActiveTake");
    GetMediaItemTake_Track = (void*(*)(void*))getReaperApi("GetMediaItemTake_Track");

    // Load position functions
    GetPlayPosition2Ex = (double(*)(void*))getReaperApi("GetPlayPosition2Ex");
    GetCursorPositionEx = (double(*)(void*))getReaperApi("GetCursorPositionEx");
    GetPlayState = (int(*)())getReaperApi("GetPlayState");

    // Load MIDI functions
    MIDI_CountEvts = (int(*)(void*, int*, int*, int*))getReaperApi("MIDI_CountEvts");
    MIDI_GetNote = (bool(*)(void*, int, bool*, bool*, double*, double*, int*, int*, int*))getReaperApi("MIDI_GetNote");
    MIDI_GetProjQNFromPPQPos = (double(*)(void*, double))getReaperApi("MIDI_GetProjQNFromPPQPos");

    // Load TimeMap functions for bar/beat calculations
    TimeMap2_QNToTime = (double(*)(void*, double))getReaperApi("TimeMap2_QNToTime");
    TimeMap2_timeToQN = (double(*)(void*, double))getReaperApi("TimeMap2_timeToQN");
    TimeMap2_timeToBeats = (double(*)(void*, double, int*, int*, double*, int*))getReaperApi("TimeMap2_timeToBeats");
    TimeMap_GetDividedBpmAtTime = (double(*)(double))getReaperApi("TimeMap_GetDividedBpmAtTime");

    // Check that critical functions loaded successfully
    bool success = (CountMediaItems != nullptr &&
                   GetMediaItem != nullptr &&
                   GetActiveTake != nullptr &&
                   GetPlayPosition2Ex != nullptr &&
                   GetCursorPositionEx != nullptr &&
                   GetPlayState != nullptr &&
                   MIDI_CountEvts != nullptr &&
                   MIDI_GetNote != nullptr &&
                   MIDI_GetProjQNFromPPQPos != nullptr &&
                   TimeMap2_QNToTime != nullptr &&
                   TimeMap2_timeToBeats != nullptr);

    return success;
}

std::vector<ReaperMidiProvider::ReaperMidiNote> ReaperMidiProvider::getNotesInRange(double startPPQ, double endPPQ, int trackIndex)
{
    std::vector<ReaperMidiNote> notes;

    if (!reaperApiInitialized)
    {
        if (logger) logger->log(DebugTools::LogCategory::ReaperAPI, "API not initialized");
        return notes;
    }

    juce::ScopedLock lock(apiLock);

    try
    {
        // REAPER stores MIDI at 960 PPQ per quarter note internally
        // VST playhead reports in quarter notes (1 PPQ = 1 QN)
        // Convert our query range TO REAPER's 960 PPQ system for comparison
        const double REAPER_PPQ_RESOLUTION = 960.0;
        double reaperStartPPQ = startPPQ * REAPER_PPQ_RESOLUTION;
        double reaperEndPPQ = endPPQ * REAPER_PPQ_RESOLUTION;

        // Minimal logging - only on significant position changes
        static double lastLoggedStart = -1000.0;
        bool shouldLog = std::abs(startPPQ - lastLoggedStart) > 5.0; // Only log every 5 QN
        if (logger && shouldLog)
        {
            logger->log(DebugTools::LogCategory::ReaperAPI,
                       "=== Query @ " + juce::String(startPPQ, 1) + " QN ===");
            lastLoggedStart = startPPQ;
        }

        // Get current project
        auto EnumProjects = (void*(*)(int, char*, int))getReaperApi("EnumProjects");
        if (!EnumProjects)
        {
            if (logger) logger->log(DebugTools::LogCategory::ReaperAPI, "ERROR: Could not get EnumProjects API function");
            return notes;
        }

        void* project = EnumProjects(-1, nullptr, 0);
        if (!project)
        {
            if (logger) logger->log(DebugTools::LogCategory::ReaperAPI, "ERROR: Could not get current project");
            return notes;
        }

        // For now, just hardcode back to track index 1 (track 2 in UI)
        // TODO: Make this configurable or auto-detect properly
        auto GetTrack = (void*(*)(void*, int))getReaperApi("GetTrack");
        if (!GetTrack)
        {
            if (logger) logger->log(DebugTools::LogCategory::ReaperAPI, "ERROR: Could not get GetTrack API function");
            return notes;
        }

        // Use provided track index, or auto-detect if not specified
        int targetTrackIndex = trackIndex;
        if (targetTrackIndex < 0)
        {
            // Auto-detect which track this plugin instance is on
            targetTrackIndex = ReaperTrackDetector::detectPluginTrack(
                [this](const char* funcname) -> void* { return getReaperApi(funcname); }
            );

            // If detection failed, default to track 0
            if (targetTrackIndex < 0)
            {
                targetTrackIndex = 0;
                if (logger) logger->log(DebugTools::LogCategory::ReaperAPI, "WARNING: Could not detect plugin track, defaulting to track 0");
            }
            else
            {
                if (logger)
                {
                    static int lastLoggedTrack = -1;
                    if (targetTrackIndex != lastLoggedTrack)
                    {
                        logger->log(DebugTools::LogCategory::ReaperAPI,
                                   "Auto-detected plugin on track index " + juce::String(targetTrackIndex) +
                                   " (Track " + juce::String(targetTrackIndex + 1) + " in UI)");
                        lastLoggedTrack = targetTrackIndex;
                    }
                }
            }
        }
        else
        {
            if (logger)
            {
                static int lastLoggedTrack = -1;
                if (targetTrackIndex != lastLoggedTrack)
                {
                    logger->log(DebugTools::LogCategory::ReaperAPI,
                               "Using configured track index " + juce::String(targetTrackIndex) +
                               " (Track " + juce::String(targetTrackIndex + 1) + " in UI)");
                    lastLoggedTrack = targetTrackIndex;
                }
            }
        }

        void* targetTrack = GetTrack(project, targetTrackIndex);
        if (!targetTrack)
        {
            if (logger) logger->log(DebugTools::LogCategory::ReaperAPI, "ERROR: Could not get track at index " + juce::String(targetTrackIndex));
            return notes;
        }

        // Count media items in project
        int itemCount = CountMediaItems(project);

        int itemsChecked = 0;
        int itemsOnTargetTrack = 0;
        int midiItemsOnTargetTrack = 0;

        // Iterate through all media items
        for (int itemIdx = 0; itemIdx < itemCount; itemIdx++)
        {
            void* item = GetMediaItem(project, itemIdx);
            if (!item)
            {
                if (logger) logger->log(DebugTools::LogCategory::ReaperAPI, "  Item " + juce::String(itemIdx) + ": NULL item pointer");
                continue;
            }
            itemsChecked++;

            // Get the active take for this item
            void* take = GetActiveTake(item);
            if (!take)
            {
                if (logger) logger->log(DebugTools::LogCategory::ReaperAPI, "  Item " + juce::String(itemIdx) + ": No active take");
                continue;
            }

            // Check if this item belongs to our target track
            void* itemTrack = GetMediaItemTake_Track(take);

            if (itemTrack != targetTrack)
            {
                continue; // Skip items not on target track
            }

            itemsOnTargetTrack++;

            // Check if this is a MIDI take by counting MIDI events
            int noteCount = 0, ccCount = 0, sysexCount = 0;
            int countResult = MIDI_CountEvts(take, &noteCount, &ccCount, &sysexCount);

            if (countResult == 0)
            {
                continue; // Not a MIDI take
            }

            if (noteCount == 0)
            {
                continue; // No MIDI notes
            }

            midiItemsOnTargetTrack++;

            // Read all MIDI notes from this take and convert directly to project QN
            int notesInRange = 0;
            for (int noteIdx = 0; noteIdx < noteCount; noteIdx++)
            {
                bool selected = false, muted = false;
                double noteStartPPQ = 0.0, noteEndPPQ = 0.0;
                int channel = 0, pitch = 0, velocity = 0;

                if (MIDI_GetNote(take, noteIdx, &selected, &muted,
                               &noteStartPPQ, &noteEndPPQ, &channel, &pitch, &velocity))
                {
                    // Convert take-relative PPQ positions directly to project quarter notes
                    // This handles ALL complexity: item position, tempo changes, time signatures, etc.
                    double projectStartQN = MIDI_GetProjQNFromPPQPos(take, noteStartPPQ);
                    double projectEndQN = MIDI_GetProjQNFromPPQPos(take, noteEndPPQ);

                    // Check if note overlaps with requested time range (in quarter notes)
                    if (projectEndQN >= startPPQ && projectStartQN <= endPPQ)
                    {
                        notesInRange++;

                        ReaperMidiNote note;
                        note.startPPQ = projectStartQN;
                        note.endPPQ = projectEndQN;
                        note.channel = channel;
                        note.pitch = pitch;
                        note.velocity = velocity;
                        note.selected = selected;
                        note.muted = muted;

                        notes.push_back(note);
                    }
                }
            }

            // Log which item we processed (only if it had notes in range)
            if (logger && shouldLog && notesInRange > 0)
            {
                logger->log(DebugTools::LogCategory::ReaperAPI,
                           "  Found " + juce::String(notesInRange) + " notes from MIDI item with " +
                           juce::String(noteCount) + " total notes");
            }

        }

        // Log summary only if we found notes or had issues
        if (logger && shouldLog)
        {
            logger->log(DebugTools::LogCategory::ReaperAPI,
                       "  Found " + juce::String(notes.size()) + " notes from " +
                       juce::String(midiItemsOnTargetTrack) + " MIDI items");
        }

    }
    catch (...)
    {
        // Silently handle exceptions - just return empty results
    }

    return notes;
}

double ReaperMidiProvider::getCurrentPlayPosition()
{
    if (!reaperApiInitialized || !GetPlayPosition2Ex)
        return 0.0;

    juce::ScopedLock lock(apiLock);

    try
    {
        // Get current project
        auto EnumProjects = (void*(*)(int, char*, int))getReaperApi("EnumProjects");
        if (!EnumProjects) return 0.0;

        void* project = EnumProjects(-1, nullptr, 0);
        if (!project) return 0.0;

        return GetPlayPosition2Ex(project);
    }
    catch (...)
    {
        return 0.0;
    }
}

double ReaperMidiProvider::getCurrentCursorPosition()
{
    if (!reaperApiInitialized || !GetCursorPositionEx)
        return 0.0;

    juce::ScopedLock lock(apiLock);

    try
    {
        // Get current project
        auto EnumProjects = (void*(*)(int, char*, int))getReaperApi("EnumProjects");
        if (!EnumProjects) return 0.0;

        void* project = EnumProjects(-1, nullptr, 0);
        if (!project) return 0.0;

        return GetCursorPositionEx(project);
    }
    catch (...)
    {
        return 0.0;
    }
}

bool ReaperMidiProvider::isPlaying()
{
    if (!reaperApiInitialized || !GetPlayState)
        return false;

    juce::ScopedLock lock(apiLock);

    try
    {
        int playState = GetPlayState();
        return (playState & 1) != 0; // Bit 0 = playing
    }
    catch (...)
    {
        return false;
    }
}

bool ReaperMidiProvider::isTrackMidiTrack(void* track)
{
    // This could be enhanced to check track properties
    // For now, we'll determine this by checking if items contain MIDI
    return true; // Simplified for now
}

bool ReaperMidiProvider::isItemInTimeRange(void* item, double startPPQ, double endPPQ)
{
    // This could be enhanced to check item timing
    // For now, we check each take's MIDI content directly
    return true; // Simplified for now
}

ReaperMidiProvider::MusicalPosition ReaperMidiProvider::getMusicalPositionAtPPQ(double ppq)
{
    MusicalPosition result;
    result.measure = 0;
    result.beatInMeasure = 0.0;
    result.fullBeats = ppq;  // Fallback: treat PPQ as beats
    result.timesig_num = 4;
    result.timesig_denom = 4;
    result.bpm = 120.0;

    if (!reaperApiInitialized || !TimeMap2_QNToTime || !TimeMap2_timeToBeats)
        return result;

    juce::ScopedLock lock(apiLock);

    try
    {
        // Get current project
        auto EnumProjects = (void*(*)(int, char*, int))getReaperApi("EnumProjects");
        if (!EnumProjects) return result;

        void* project = EnumProjects(-1, nullptr, 0);
        if (!project) return result;

        // Convert QN (our PPQ) to time in seconds
        double timeInSeconds = TimeMap2_QNToTime(project, ppq);

        // Query REAPER for the musical position AT THIS EXACT TIME
        // This accounts for ALL time signature changes before this point
        int measure = 0;
        int cml = 4;  // current measure length (time sig numerator)
        double fullbeats = 0.0;
        int cdenom = 4;  // time sig denominator

        double beatInMeasure = TimeMap2_timeToBeats(project, timeInSeconds,
                                                      &measure, &cml, &fullbeats, &cdenom);

        result.measure = measure;
        result.beatInMeasure = beatInMeasure;
        result.fullBeats = fullbeats;
        result.timesig_num = cml;
        result.timesig_denom = cdenom;

        // Get BPM at this time
        if (TimeMap_GetDividedBpmAtTime)
        {
            result.bpm = TimeMap_GetDividedBpmAtTime(timeInSeconds);
        }
    }
    catch (...)
    {
        // Return fallback result on error
    }

    return result;
}