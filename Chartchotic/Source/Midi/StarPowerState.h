#pragma once

#include <algorithm>
#include <vector>
#include "Utils/PPQ.h"
#include "Providers/REAPER/MidiCache.h"

// Star Power phrases for one track, so a point query is a binary search and not a REAPER
// scan. Built and refetched with the note cache, so it cannot drift from it. Phrases are
// assumed non-overlapping, same as the resolver's ModifierRanges.
class StarPowerState
{
public:
    struct Phrase {
        PPQ start;
        PPQ end;
    };

    void buildFromNotes(const std::vector<MidiCache::CachedNote>& notes, int spPitch)
    {
        phrases.clear();
        if (spPitch < 0) return;

        for (const auto& n : notes)
            if ((int)n.pitch == spPitch)
                phrases.push_back({ n.startPPQ, n.endPPQ });

        std::sort(phrases.begin(), phrases.end(),
                  [](const Phrase& a, const Phrase& b) { return a.start < b.start; });
    }

    bool isActiveAt(PPQ position) const
    {
        auto it = std::upper_bound(phrases.begin(), phrases.end(), position,
            [](PPQ pos, const Phrase& p) { return pos < p.start; });
        if (it == phrases.begin()) return false;
        --it;
        return position >= it->start && position < it->end;
    }

    bool hasPhrases() const { return !phrases.empty(); }

private:
    std::vector<Phrase> phrases;
};
