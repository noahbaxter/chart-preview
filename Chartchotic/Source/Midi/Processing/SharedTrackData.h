/*
  ==============================================================================

    SharedTrackData.h
    Intermediate extraction from noteStateMapArray for lockless per-difficulty
    gem resolution. Built once per frame per track under one lock.

  ==============================================================================
*/

#pragma once

#include <vector>
#include <map>
#include <array>
#include "../Utils/PPQ.h"
#include "../Utils/MidiTypes.h"

struct RawNoteEvent {
    uint8_t pitch;
    uint8_t velocity;
};

struct ModifierRange {
    PPQ start;
    PPQ end;
};

struct ModifierRanges {
    std::vector<ModifierRange> starPower;
    std::vector<ModifierRange> tap;
    std::array<std::vector<ModifierRange>, 4> hopoForce;    // indexed by SkillLevel
    std::array<std::vector<ModifierRange>, 4> strumForce;    // indexed by SkillLevel
    std::vector<ModifierRange> tomYellow;
    std::vector<ModifierRange> tomBlue;
    std::vector<ModifierRange> tomGreen;
    std::array<std::vector<ModifierRange>, 4> hihatPedal;        // elite: Pedal Down under Yellow => Closed hat (indexed by SkillLevel)
    std::array<std::vector<ModifierRange>, 4> hihatIndifferent;  // elite: Indifferent marker over Yellow => Indifferent hat
    std::array<std::vector<ModifierRange>, 4> flam;              // elite: Flam marker => the hand gem under it is a flam (indexed by SkillLevel)

    static bool isActiveAt(const std::vector<ModifierRange>& ranges, PPQ position)
    {
        if (ranges.empty()) return false;
        auto it = std::upper_bound(ranges.begin(), ranges.end(), position,
            [](PPQ pos, const ModifierRange& r) { return pos < r.start; });
        if (it == ranges.begin()) return false;
        --it;
        return position >= it->start && position < it->end;
    }
};

struct RawSustainPair {
    PPQ startPPQ;
    PPQ endPPQ;
    uint8_t pitch;
    uint8_t velocity;
};

struct RawLaneMarker {
    PPQ startPPQ;
    PPQ endPPQ;
    uint8_t laneType;
    uint8_t laneVelocity;
};

struct SharedWindow {
    std::map<PPQ, std::vector<RawNoteEvent>> positions;
    ModifierRanges modifiers;
    std::vector<RawSustainPair> sustains;
    std::vector<RawLaneMarker> lanes;
};
