/*
  ==============================================================================

    ReaperNoteFetcher.cpp
    Implementation of REAPER MIDI note extraction

  ==============================================================================
*/

#include "ReaperNoteFetcher.h"
#include "ReaperApiHelpers.h"
#include "../../../REAPER/ReaperTrackDetector.h"

ReaperNoteFetcher::ReaperNoteFetcher(std::function<void*(const char*)> reaperApiFunc, const ReaperAPIs& apis)
    : getReaperApi(reaperApiFunc), apis(apis)
{
}

ReaperNoteFetcher::~ReaperNoteFetcher()
{
}

std::vector<ReaperMidiProvider::ReaperMidiNote> ReaperNoteFetcher::fetchAllNotes(int trackIndex)
{
    return fetchNotesInRange(-std::numeric_limits<double>::infinity(),
                            std::numeric_limits<double>::infinity(),
                            trackIndex);
}

std::vector<ReaperMidiProvider::ReaperMidiNote> ReaperNoteFetcher::fetchNotesInRange(double startPPQ, double endPPQ, int trackIndex)
{
    std::vector<ReaperMidiProvider::ReaperMidiNote> notes;

    if (!apis.isLoaded() || !getReaperApi)
        return notes;

    try
    {
        juce::ScopedLock lock(apiLock);

        void* project = ReaperApiHelpers::getProject(getReaperApi);
        if (!project) return notes;

        void* targetTrack = getTargetTrack(project, trackIndex);
        if (!targetTrack) return notes;

        return iterateAndExtractNotes(project, targetTrack, startPPQ, endPPQ, true);
    }
    catch (...)
    {
        return notes;
    }
}

void* ReaperNoteFetcher::getTargetTrack(void* project, int& trackIndex)
{
    if (!project || !apis.GetTrack)
        return nullptr;

    // Auto-detect if not specified
    if (trackIndex < 0 && getReaperApi)
    {
        trackIndex = ReaperTrackDetector::detectPluginTrack(getReaperApi);
        if (trackIndex < 0)
            trackIndex = 0;
    }

    return apis.GetTrack(project, trackIndex);
}

std::vector<ReaperMidiProvider::ReaperMidiNote> ReaperNoteFetcher::iterateAndExtractNotes(
    void* project,
    void* targetTrack,
    double startPPQ,
    double endPPQ,
    bool filterByRange)
{
    std::vector<ReaperMidiProvider::ReaperMidiNote> notes;

    if (!project || !targetTrack || !apis.CountMediaItems || !apis.GetMediaItem)
        return notes;

    int itemCount = apis.CountMediaItems(project);

    for (int itemIdx = 0; itemIdx < itemCount; itemIdx++)
    {
        void* item = apis.GetMediaItem(project, itemIdx);
        if (!item) continue;

        void* take = apis.GetActiveTake(item);
        if (!take) continue;

        // Check track membership
        void* itemTrack = apis.GetMediaItemTake_Track(take);
        if (itemTrack != targetTrack) continue;

        // Check if MIDI take
        int noteCount = 0, ccCount = 0, sysexCount = 0;
        if (!apis.MIDI_CountEvts || apis.MIDI_CountEvts(take, &noteCount, &ccCount, &sysexCount) == 0 || noteCount == 0)
            continue;

        // Extract notes from this take
        extractNotesFromTake(take, notes, filterByRange ? startPPQ : -std::numeric_limits<double>::infinity(),
                            filterByRange ? endPPQ : std::numeric_limits<double>::infinity());
    }

    return notes;
}

void ReaperNoteFetcher::extractNotesFromTake(void* take,
                                            std::vector<ReaperMidiProvider::ReaperMidiNote>& outNotes,
                                            double startPPQ,
                                            double endPPQ)
{
    if (!take || !apis.MIDI_GetNote || !apis.MIDI_GetProjQNFromPPQPos)
        return;

    int noteCount = 0, ccCount = 0, sysexCount = 0;
    if (!apis.MIDI_CountEvts || apis.MIDI_CountEvts(take, &noteCount, &ccCount, &sysexCount) == 0)
        return;

    for (int noteIdx = 0; noteIdx < noteCount; noteIdx++)
    {
        bool selected = false, muted = false;
        double noteStartPPQ = 0.0, noteEndPPQ = 0.0;
        int channel = 0, pitch = 0, velocity = 0;

        if (!apis.MIDI_GetNote(take, noteIdx, &selected, &muted, &noteStartPPQ, &noteEndPPQ, &channel, &pitch, &velocity))
            continue;

        // Convert to project QN
        double projectStartQN = apis.MIDI_GetProjQNFromPPQPos(take, noteStartPPQ);
        double projectEndQN = apis.MIDI_GetProjQNFromPPQPos(take, noteEndPPQ);

        // Filter by range if needed
        if (projectEndQN < startPPQ || projectStartQN > endPPQ)
            continue;

        ReaperMidiProvider::ReaperMidiNote note;
        note.startPPQ = projectStartQN;
        note.endPPQ = projectEndQN;
        note.channel = channel;
        note.pitch = pitch;
        note.velocity = velocity;
        note.selected = selected;
        note.muted = muted;

        outNotes.push_back(note);
    }
}
