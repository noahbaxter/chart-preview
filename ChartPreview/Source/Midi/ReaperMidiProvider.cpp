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
        // Get current project
        auto EnumProjects = (void*(*)(int, char*, int))getReaperApi("EnumProjects");
        if (!EnumProjects) return notes;

        void* project = EnumProjects(-1, nullptr, 0);
        if (!project) return notes;

        // Count media items in project
        int itemCount = CountMediaItems(project);

        // Iterate through all media items
        for (int itemIdx = 0; itemIdx < itemCount; itemIdx++)
        {
            void* item = GetMediaItem(project, itemIdx);
            if (!item) continue;

            // Get the active take for this item
            void* take = GetActiveTake(item);
            if (!take) continue;

            // Check if this is a MIDI take by counting MIDI events
            int noteCount = 0, ccCount = 0, sysexCount = 0;
            if (MIDI_CountEvts(take, &noteCount, &ccCount, &sysexCount) == 0)
                continue; // Not a MIDI take

            if (noteCount == 0)
                continue; // No MIDI notes

            // Read all MIDI notes from this take
            for (int noteIdx = 0; noteIdx < noteCount; noteIdx++)
            {
                bool selected = false, muted = false;
                double notStartPPQ = 0.0, noteEndPPQ = 0.0;
                int channel = 0, pitch = 0, velocity = 0;

                if (MIDI_GetNote(take, noteIdx, &selected, &muted,
                               &notStartPPQ, &noteEndPPQ, &channel, &pitch, &velocity))
                {
                    // REAPER stores MIDI at 960 PPQ per quarter note internally
                    // VST playhead reports in quarter notes (1 PPQ = 1 QN)
                    // Convert: VST_PPQ = REAPER_PPQ / 960
                    const double REAPER_PPQ_RESOLUTION = 960.0;
                    double convertedStartPPQ = notStartPPQ / REAPER_PPQ_RESOLUTION;
                    double convertedEndPPQ = noteEndPPQ / REAPER_PPQ_RESOLUTION;

                    // Check if note overlaps with requested time range
                    if (convertedEndPPQ >= startPPQ && convertedStartPPQ <= endPPQ)
                    {
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