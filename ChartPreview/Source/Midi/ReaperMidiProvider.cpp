#include "ReaperMidiProvider.h"

ReaperMidiProvider::ReaperMidiProvider()
{
    DBG("ReaperMidiProvider: Initialized");
}

ReaperMidiProvider::~ReaperMidiProvider()
{
    DBG("ReaperMidiProvider: Destroyed");
}

void ReaperMidiProvider::initialize(void* (*reaperGetApiFunc)(const char*))
{
    juce::ScopedLock lock(apiLock);

    getReaperApi = reaperGetApiFunc;

    if (!getReaperApi)
    {
        DBG("ReaperMidiProvider: No REAPER API function provided");
        reaperApiInitialized = false;
        return;
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
    }
    catch (...)
    {
        DBG("ReaperMidiProvider: CRASH PREVENTED - Invalid REAPER API function pointer");
        reaperApiInitialized = false;
        getReaperApi = nullptr;
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

std::vector<ReaperMidiProvider::ReaperMidiNote> ReaperMidiProvider::getNotesInRange(double startPPQ, double endPPQ)
{
    std::vector<ReaperMidiNote> notes;

    if (!reaperApiInitialized)
        return notes;

    juce::ScopedLock lock(apiLock);

    try
    {
        // REAPER stores MIDI at 960 PPQ per quarter note internally
        // VST playhead reports in quarter notes (1 PPQ = 1 QN)
        // Convert our query range TO REAPER's 960 PPQ system for comparison
        const double REAPER_PPQ_RESOLUTION = 960.0;
        double reaperStartPPQ = startPPQ * REAPER_PPQ_RESOLUTION;
        double reaperEndPPQ = endPPQ * REAPER_PPQ_RESOLUTION;

        // Get current project
        auto EnumProjects = (void*(*)(int, char*, int))getReaperApi("EnumProjects");
        if (!EnumProjects) return notes;

        void* project = EnumProjects(-1, nullptr, 0);
        if (!project) return notes;

        // For now, just hardcode back to track index 1 (track 2 in UI)
        // TODO: Make this configurable or auto-detect properly
        auto GetTrack = (void*(*)(void*, int))getReaperApi("GetTrack");
        if (!GetTrack) return notes;

        // HARDCODED: Reading from first track (track 1 in REAPER UI)
        // Track index 0 = track 1, index 1 = track 2, etc.
        int targetTrackIndex = 0; // First track = PART DRUMS

        void* targetTrack = GetTrack(project, targetTrackIndex);
        if (!targetTrack)
        {
            juce::Logger::writeToLog("ERROR: Could not get track at index " + juce::String(targetTrackIndex));
            return notes;
        }

        juce::String targetTrackName = "Track " + juce::String(targetTrackIndex + 1);

        // Count media items in project
        int itemCount = CountMediaItems(project);

        int itemsChecked = 0;
        int itemsOnTargetTrack = 0;
        int midiItemsOnTargetTrack = 0;

        // Iterate through all media items
        for (int itemIdx = 0; itemIdx < itemCount; itemIdx++)
        {
            void* item = GetMediaItem(project, itemIdx);
            if (!item) continue;
            itemsChecked++;

            // Get the active take for this item
            void* take = GetActiveTake(item);
            if (!take) continue;

            // Check if this item belongs to our target track
            void* itemTrack = GetMediaItemTake_Track(take);
            if (itemTrack != targetTrack) continue; // Skip items not on target track

            itemsOnTargetTrack++;

            // Check if this is a MIDI take by counting MIDI events
            int noteCount = 0, ccCount = 0, sysexCount = 0;
            if (MIDI_CountEvts(take, &noteCount, &ccCount, &sysexCount) == 0)
                continue; // Not a MIDI take

            if (noteCount == 0)
                continue; // No MIDI notes

            midiItemsOnTargetTrack++;

            // Read all MIDI notes from this take
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

        // Log summary statistics (using juce::Logger since we can't use print() here)
        juce::String debugMsg = "=== REAPER MIDI Read Summary ===\n";
        debugMsg += "  Queried PPQ range: " + juce::String(startPPQ) + " to " + juce::String(endPPQ) + "\n";
        debugMsg += "  Target track index: " + juce::String(targetTrackIndex) + " (0-based) = Track " + juce::String(targetTrackIndex + 1) + " in UI\n";
        debugMsg += "  Target track name: \"" + targetTrackName + "\"\n";
        debugMsg += "  Total items in project: " + juce::String(itemCount) + "\n";
        debugMsg += "  Items checked: " + juce::String(itemsChecked) + "\n";
        debugMsg += "  Items on target track: " + juce::String(itemsOnTargetTrack) + "\n";
        debugMsg += "  MIDI items on target track: " + juce::String(midiItemsOnTargetTrack) + "\n";
        debugMsg += "  Notes found in range: " + juce::String(notes.size()) + "\n";
        juce::Logger::writeToLog(debugMsg);

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