#include "ReaperMidiProvider.h"
#include "../REAPER/ReaperTrackDetector.h"

ReaperMidiProvider::ReaperMidiProvider()
{
    DBG("ReaperMidiProvider: Initialized");
}

ReaperMidiProvider::~ReaperMidiProvider()
{
    DBG("ReaperMidiProvider: Destroyed");
}

bool ReaperMidiProvider::initialize(void* (*reaperGetApiFunc)(const char*))
{
    juce::ScopedLock lock(apiLock);

    getReaperApi = reaperGetApiFunc;

    if (!getReaperApi)
    {
        DBG("ReaperMidiProvider: No REAPER API function provided");
        reaperApiInitialized = false;
        return false;
    }

    // Add safety check to prevent crashes with invalid function pointers
    try
    {
        // Load all required REAPER API functions
        reaperApiInitialized = loadReaperApiFunctions();

        if (reaperApiInitialized)
        {
            DBG("ReaperMidiProvider: Successfully initialized with REAPER API");
        }
        else
        {
            DBG("ReaperMidiProvider: Failed to load required REAPER API functions");
        }

        return reaperApiInitialized;
    }
    catch (...)
    {
        DBG("ReaperMidiProvider: CRASH PREVENTED - Invalid REAPER API function pointer");
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

    // Check that critical functions loaded successfully
    bool success = (CountMediaItems != nullptr &&
                   GetMediaItem != nullptr &&
                   GetActiveTake != nullptr &&
                   GetPlayPosition2Ex != nullptr &&
                   GetCursorPositionEx != nullptr &&
                   GetPlayState != nullptr &&
                   MIDI_CountEvts != nullptr &&
                   MIDI_GetNote != nullptr);

    if (!success)
    {
        DBG("ReaperMidiProvider: Failed to load one or more critical REAPER API functions");
        DBG("  CountMediaItems: " << (CountMediaItems ? "OK" : "FAILED"));
        DBG("  GetMediaItem: " << (GetMediaItem ? "OK" : "FAILED"));
        DBG("  GetActiveTake: " << (GetActiveTake ? "OK" : "FAILED"));
        DBG("  GetPlayPosition2Ex: " << (GetPlayPosition2Ex ? "OK" : "FAILED"));
        DBG("  GetCursorPositionEx: " << (GetCursorPositionEx ? "OK" : "FAILED"));
        DBG("  GetPlayState: " << (GetPlayState ? "OK" : "FAILED"));
        DBG("  MIDI_CountEvts: " << (MIDI_CountEvts ? "OK" : "FAILED"));
        DBG("  MIDI_GetNote: " << (MIDI_GetNote ? "OK" : "FAILED"));
    }

    return success;
}

std::vector<ReaperMidiProvider::ReaperMidiNote> ReaperMidiProvider::getNotesInRange(double startPPQ, double endPPQ, int trackIndex)
{
    std::vector<ReaperMidiNote> notes;

    if (!reaperApiInitialized)
    {
        if (print) print("ReaperMidiProvider: API not initialized");
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
        if (print && shouldLog)
        {
            print("=== REAPER API Query (@ " + juce::String(startPPQ, 1) + " QN) ===");
            lastLoggedStart = startPPQ;
        }

        // Get current project
        auto EnumProjects = (void*(*)(int, char*, int))getReaperApi("EnumProjects");
        if (!EnumProjects)
        {
            if (print) print("ERROR: Could not get EnumProjects API function");
            return notes;
        }

        void* project = EnumProjects(-1, nullptr, 0);
        if (!project)
        {
            if (print) print("ERROR: Could not get current project");
            return notes;
        }

        // For now, just hardcode back to track index 1 (track 2 in UI)
        // TODO: Make this configurable or auto-detect properly
        auto GetTrack = (void*(*)(void*, int))getReaperApi("GetTrack");
        if (!GetTrack)
        {
            if (print) print("ERROR: Could not get GetTrack API function");
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
                if (print) print("WARNING: Could not detect plugin track, defaulting to track 0");
            }
            else
            {
                if (print)
                {
                    static int lastLoggedTrack = -1;
                    if (targetTrackIndex != lastLoggedTrack)
                    {
                        print("Auto-detected plugin on track index " + juce::String(targetTrackIndex) +
                              " (Track " + juce::String(targetTrackIndex + 1) + " in UI)");
                        lastLoggedTrack = targetTrackIndex;
                    }
                }
            }
        }
        else
        {
            if (print)
            {
                static int lastLoggedTrack = -1;
                if (targetTrackIndex != lastLoggedTrack)
                {
                    print("Using configured track index " + juce::String(targetTrackIndex) +
                          " (Track " + juce::String(targetTrackIndex + 1) + " in UI)");
                    lastLoggedTrack = targetTrackIndex;
                }
            }
        }

        void* targetTrack = GetTrack(project, targetTrackIndex);
        if (!targetTrack)
        {
            if (print) print("ERROR: Could not get track at index " + juce::String(targetTrackIndex));
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
                if (print) print("  Item " + juce::String(itemIdx) + ": NULL item pointer");
                continue;
            }
            itemsChecked++;

            // Get the active take for this item
            void* take = GetActiveTake(item);
            if (!take)
            {
                if (print) print("  Item " + juce::String(itemIdx) + ": No active take");
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

            // Read all MIDI notes from this take
            int notesInRange = 0;
            for (int noteIdx = 0; noteIdx < noteCount; noteIdx++)
            {
                bool selected = false, muted = false;
                double notStartPPQ = 0.0, noteEndPPQ = 0.0;
                int channel = 0, pitch = 0, velocity = 0;

                if (MIDI_GetNote(take, noteIdx, &selected, &muted,
                               &notStartPPQ, &noteEndPPQ, &channel, &pitch, &velocity))
                {
                    // Check if note overlaps with requested time range (in REAPER's 960 PPQ)
                    if (noteEndPPQ >= reaperStartPPQ && notStartPPQ <= reaperEndPPQ)
                    {
                        notesInRange++;

                        // Convert REAPER's 960 PPQ to VST quarter notes for return
                        double convertedStartPPQ = notStartPPQ / REAPER_PPQ_RESOLUTION;
                        double convertedEndPPQ = noteEndPPQ / REAPER_PPQ_RESOLUTION;

                        ReaperMidiNote note;
                        note.startPPQ = convertedStartPPQ;
                        note.endPPQ = convertedEndPPQ;
                        note.channel = channel;
                        note.pitch = pitch;
                        note.velocity = velocity;
                        note.selected = selected;
                        note.muted = muted;

                        notes.push_back(note);
                    }
                }
            }

        }

        // Log summary only if we found notes or had issues
        if (print && shouldLog)
        {
            print("  Found " + juce::String(notes.size()) + " notes from " + juce::String(midiItemsOnTargetTrack) + " MIDI items");
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