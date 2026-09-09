#include "InstrumentSession.h"

const std::vector<MidiCache::CachedNote> InstrumentSession::emptyNotes;
const TrackTextEvents InstrumentSession::emptyTextEvents;
const DiscoFlipState InstrumentSession::emptyDiscoFlip;

InstrumentSession::InstrumentSession(std::unique_ptr<TrackDiscovery> disc,
                                     std::unique_ptr<TrackNoteProvider> prov)
    : discovery(std::move(disc)), provider(std::move(prov))
{
}

void InstrumentSession::discover()
{
    tracks = discovery->discoverTracks();
    trackData.resize(tracks.size());

    for (int i = 0; i < (int)tracks.size(); ++i)
        fetchTrackData(i);
}

bool InstrumentSession::pollForChanges()
{
    // Re-run discovery to detect added/removed/renamed tracks
    auto freshTracks = discovery->discoverTracks();
    bool structureChanged = freshTracks.size() != tracks.size();
    if (!structureChanged)
    {
        for (int i = 0; i < (int)tracks.size(); ++i)
        {
            if (freshTracks[i].part != tracks[i].part
                || freshTracks[i].sourceTrackIndex != tracks[i].sourceTrackIndex)
            {
                structureChanged = true;
                break;
            }
        }
    }

    if (structureChanged)
    {
        tracks = std::move(freshTracks);
        trackData.resize(tracks.size());
        for (int i = 0; i < (int)tracks.size(); ++i)
            fetchTrackData(i);
        return true;
    }

    // No structural change — just check for note edits on existing tracks
    bool anyChanged = false;
    for (int i = 0; i < (int)tracks.size(); ++i)
    {
        std::string currentHash = provider->getTrackHash(tracks[i]);
        if (currentHash != trackData[i].lastHash)
        {
            fetchTrackData(i);
            anyChanged = true;
        }
    }
    return anyChanged;
}

const std::vector<MidiCache::CachedNote>& InstrumentSession::getNotes(int trackIdx) const
{
    if (trackIdx < 0 || trackIdx >= (int)trackData.size())
        return emptyNotes;
    return trackData[trackIdx].notes;
}

const TrackTextEvents& InstrumentSession::getTextEvents(int trackIdx) const
{
    if (trackIdx < 0 || trackIdx >= (int)trackData.size())
        return emptyTextEvents;
    return trackData[trackIdx].textEvents;
}

const DiscoFlipState& InstrumentSession::getDiscoFlipState(int trackIdx) const
{
    if (trackIdx < 0 || trackIdx >= (int)trackData.size())
        return emptyDiscoFlip;
    return trackData[trackIdx].discoFlipState;
}

void InstrumentSession::setDiscovery(std::unique_ptr<TrackDiscovery> newDiscovery)
{
    discovery = std::move(newDiscovery);
}

void InstrumentSession::fetchTrackData(int idx)
{
    auto& td = trackData[idx];
    td.notes = provider->fetchNotes(tracks[idx]);
    td.textEvents = provider->fetchTextEvents(tracks[idx]);
    td.discoFlipState.buildFromTextEvents(td.textEvents);
    td.lastHash = provider->getTrackHash(tracks[idx]);
}

void InstrumentSession::invalidateTrack(int trackIdx)
{
    if (trackIdx < 0 || trackIdx >= (int)trackData.size())
        return;
    fetchTrackData(trackIdx);
}
