#pragma once

#include <JuceHeader.h>
#include <vector>
#include <algorithm>
#include "../../UI/ControlConstants.h"

struct InstrumentTrackInfo {
    Part part;
    int sourceTrackIndex;    // Backend-opaque: REAPER track idx, MIDI file track idx, etc.
    juce::String trackName;  // "PART GUITAR", "PART DRUMS", etc.
};

// Shared track name → Part matching (used by all REAPER discovery implementations)
inline bool matchTrackNameToPart(const std::string& name, Part& outPart)
{
    // Normalize: uppercase and treat '_' and ' ' as equivalent, so a track named
    // "PART ELITE DRUMS" matches the canonical "PART ELITE_DRUMS" (and vice versa).
    auto norm = [](std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return c == '_' ? ' ' : (char)::toupper(c); });
        return s;
    };
    std::string key = norm(name);

    for (auto& entry : getImplementedTrackNames())
        if (key == norm(entry.name)) { outPart = entry.part; return true; }
    for (auto& entry : getUnimplementedTrackNames())
        if (key == norm(entry.name)) { outPart = entry.part; return true; }

    return false;
}

class TrackDiscovery {
public:
    virtual ~TrackDiscovery() = default;
    virtual std::vector<InstrumentTrackInfo> discoverTracks() = 0;
};
