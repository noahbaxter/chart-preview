#pragma once

#include <JuceHeader.h>
#include <memory>
#include <vector>
#include "Discovery/TrackDiscovery.h"
#include "Providers/TrackNoteProvider.h"
#include "DiscoFlipState.h"

class InstrumentSession
{
public:
    InstrumentSession(std::unique_ptr<TrackDiscovery> discovery,
                      std::unique_ptr<TrackNoteProvider> provider);

    // Run discovery and fetch notes/text for all discovered tracks
    void discover();

    // Hash-check each track, refetch if changed. Returns true if any changed.
    bool pollForChanges();

    const std::vector<InstrumentTrackInfo>& getTracks() const { return tracks; }

    const std::vector<MidiCache::CachedNote>& getNotes(int trackIdx) const;
    const TrackTextEvents& getTextEvents(int trackIdx) const;
    const DiscoFlipState& getDiscoFlipState(int trackIdx) const;

    int getTrackCount() const { return (int)tracks.size(); }
    bool isEmpty() const { return tracks.empty(); }

    // Replace the discovery strategy (e.g. switching between Global and Local)
    void setDiscovery(std::unique_ptr<TrackDiscovery> newDiscovery);

    // Force a refetch of one track's notes/text (e.g. after authoring writes).
    // Does NOT re-run discovery — only refreshes data for an existing track index.
    void invalidateTrack(int trackIdx);

private:
    struct TrackData {
        std::vector<MidiCache::CachedNote> notes;
        TrackTextEvents textEvents;
        DiscoFlipState discoFlipState;
        std::string lastHash;
    };

    void fetchTrackData(int idx);

    std::unique_ptr<TrackDiscovery> discovery;
    std::unique_ptr<TrackNoteProvider> provider;
    std::vector<InstrumentTrackInfo> tracks;
    std::vector<TrackData> trackData;

    // Empty fallbacks for out-of-range access
    static const std::vector<MidiCache::CachedNote> emptyNotes;
    static const TrackTextEvents emptyTextEvents;
    static const DiscoFlipState emptyDiscoFlip;
};
