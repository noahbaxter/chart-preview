#include "ReaperMidiProvider.h"
#include "ReaperNoteFetcher.h"
#include "ReaperApiHelpers.h"
#include "../../../REAPER/ReaperTrackDetector.h"

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
        reaperApiInitialized = ReaperApiHelpers::loadAPIs(getReaperApi, apis);

        // Create note fetcher with shared APIs reference and REAPER API function getter
        if (reaperApiInitialized)
        {
            noteFetcher = std::make_unique<ReaperNoteFetcher>(getReaperApi, apis);
            if (logger)
                noteFetcher->setLogger(logger);
        }

        return reaperApiInitialized;
    }
    catch (...)
    {
        reaperApiInitialized = false;
        getReaperApi = nullptr;
        return false;
    }
}

void ReaperMidiProvider::processTempoMarkers(void* project, std::vector<TempoTimeSignatureEvent>& events)
{
    if (!apis.CountTempoTimeSigMarkers || !apis.GetTempoTimeSigMarker)
        return;

    int markerCount = apis.CountTempoTimeSigMarkers(project);
    if (markerCount == 0)
    {
        events.push_back(TempoTimeSignatureEvent(PPQ(0.0), 120.0, 4, 4));
        return;
    }

    double currentBpm = 120.0;
    int currentTimeSigNum = 4;
    int currentTimeSigDenom = 4;

    for (int markerIdx = 0; markerIdx < markerCount; markerIdx++)
    {
        double timepos = 0.0, bpm = 120.0;
        int measurepos = 0, timesig_num = 4, timesig_denom = 4;
        double beatpos = 0.0;
        bool lineartempo = false;

        if (!apis.GetTempoTimeSigMarker(project, markerIdx, &timepos, &measurepos, &beatpos,
                                   &bpm, &timesig_num, &timesig_denom, &lineartempo))
            continue;

        double ppq = apis.TimeMap2_timeToQN(project, timepos);
        bool timeSigReset = false;

        if (bpm > 0.0)
            currentBpm = bpm;
        if (timesig_num > 0 && timesig_denom > 0)
        {
            currentTimeSigNum = timesig_num;
            currentTimeSigDenom = timesig_denom;
            timeSigReset = true;
        }

        events.push_back(TempoTimeSignatureEvent(PPQ(ppq), currentBpm, currentTimeSigNum, currentTimeSigDenom, timeSigReset));
    }

    if (logger)
        logger->log(DebugTools::LogCategory::ReaperAPI,
                   "Found " + juce::String(events.size()) + " tempo/timesig markers");
}

ReaperMidiProvider::MusicalPosition ReaperMidiProvider::queryMusicalPositionFromReaper(
    void* project, double ppq, double timeInSeconds) const
{
    MusicalPosition result;
    result.measure = 0;
    result.beatInMeasure = 0.0;
    result.fullBeats = ppq;
    result.timesig_num = 4;
    result.timesig_denom = 4;
    result.bpm = 120.0;

    int measure = 0, cml = 4, cdenom = 4;
    double fullbeats = 0.0;

    result.beatInMeasure = apis.TimeMap2_timeToBeats(project, timeInSeconds,
                                                &measure, &cml, &fullbeats, &cdenom);

    result.measure = measure;
    result.fullBeats = fullbeats;
    result.timesig_num = cml;
    result.timesig_denom = cdenom;

    if (apis.TimeMap_GetDividedBpmAtTime)
        result.bpm = apis.TimeMap_GetDividedBpmAtTime(timeInSeconds);

    return result;
}

std::vector<TempoTimeSignatureEvent> ReaperMidiProvider::getAllTempoTimeSignatureEvents()
{
    std::vector<TempoTimeSignatureEvent> events;

    if (!reaperApiInitialized || !apis.TimeMap2_QNToTime)
        return events;

    juce::ScopedLock lock(apiLock);
    try
    {
        void* project = ReaperApiHelpers::getProject(getReaperApi);
        if (project)
            processTempoMarkers(project, events);
    }
    catch (...)
    {
        // Silently handle exceptions
    }

    return events;
}

std::vector<ReaperMidiProvider::ReaperMidiNote> ReaperMidiProvider::getAllNotesFromTrack(int trackIndex)
{
    if (!noteFetcher)
        return {};

    return noteFetcher->fetchAllNotes(trackIndex);
}

double ReaperMidiProvider::getCurrentPlayPosition()
{
    if (!reaperApiInitialized || !apis.GetPlayPosition2Ex)
        return 0.0;

    return ReaperApiHelpers::performQuery(
        getReaperApi,
        reaperApiInitialized,
        apiLock,
        [this](void* project) { return apis.GetPlayPosition2Ex(project); },
        0.0
    );
}

double ReaperMidiProvider::getCurrentCursorPosition()
{
    if (!reaperApiInitialized || !apis.GetCursorPositionEx)
        return 0.0;

    return ReaperApiHelpers::performQuery(
        getReaperApi,
        reaperApiInitialized,
        apiLock,
        [this](void* project) { return apis.GetCursorPositionEx(project); },
        0.0
    );
}

bool ReaperMidiProvider::isPlaying()
{
    if (!reaperApiInitialized || !apis.GetPlayState)
        return false;

    juce::ScopedLock lock(apiLock);

    try
    {
        int playState = apis.GetPlayState();
        return (playState & 1) != 0; // Bit 0 = playing
    }
    catch (...)
    {
        return false;
    }
}

ReaperMidiProvider::MusicalPosition ReaperMidiProvider::getMusicalPositionAtPPQ(double ppq)
{
    MusicalPosition result;
    result.measure = 0;
    result.beatInMeasure = 0.0;
    result.fullBeats = ppq;
    result.timesig_num = 4;
    result.timesig_denom = 4;
    result.bpm = 120.0;

    if (!reaperApiInitialized || !apis.TimeMap2_QNToTime || !apis.TimeMap2_timeToBeats)
        return result;

    juce::ScopedLock lock(apiLock);
    try
    {
        void* project = ReaperApiHelpers::getProject(getReaperApi);
        if (!project) return result;

        double timeInSeconds = apis.TimeMap2_QNToTime(project, ppq);
        result = queryMusicalPositionFromReaper(project, ppq, timeInSeconds);
    }
    catch (...)
    {
        // Return fallback result on error
    }

    return result;
}

double ReaperMidiProvider::ppqToTime(double ppq)
{
    if (!reaperApiInitialized || !apis.TimeMap2_QNToTime)
        return ppq * (60.0 / 120.0);  // Default 120 BPM fallback

    return ReaperApiHelpers::performQuery(
        getReaperApi,
        reaperApiInitialized,
        apiLock,
        [this, ppq](void* project) { return apis.TimeMap2_QNToTime(project, ppq); },
        ppq * (60.0 / 120.0)
    );
}

std::string ReaperMidiProvider::getTrackHash(int trackIndex, bool notesonly)
{
    std::string emptyHash;

    if (!reaperApiInitialized || !apis.MIDI_GetTrackHash)
        return emptyHash;

    juce::ScopedLock lock(apiLock);

    try
    {
        void* project = ReaperApiHelpers::getProject(getReaperApi);
        if (!project) return emptyHash;

        void* track = apis.GetTrack(project, trackIndex);
        if (!track) return emptyHash;

        // Get hash for this track
        char hashBuffer[256];
        if (apis.MIDI_GetTrackHash(track, notesonly, hashBuffer, sizeof(hashBuffer)))
            return std::string(hashBuffer);

        return emptyHash;
    }
    catch (...)
    {
        return emptyHash;
    }
}

// ============ DEPRECATED METHODS ============

// DEPRECATED: Use getAllNotesFromTrack or the noteFetcher directly
// Kept for API compatibility but delegates to noteFetcher with range filtering
std::vector<ReaperMidiProvider::ReaperMidiNote> ReaperMidiProvider::getNotesInRange(double startPPQ, double endPPQ, int trackIndex)
{
    // This method is deprecated. In new code, use:
    // - getAllNotesFromTrack() for bulk fetching (recommended)
    // - Or access noteFetcher directly for windowed fetches

    if (!noteFetcher)
        return {};

    return noteFetcher->fetchNotesInRange(startPPQ, endPPQ, trackIndex);
}
