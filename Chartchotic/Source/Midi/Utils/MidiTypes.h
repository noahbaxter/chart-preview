/*
  ==============================================================================

    MidiTypes.h
    Created: 18 Oct 2024
    Author:  Noah Baxter

    MIDI-specific type definitions including note data, pitch mappings, and dynamics.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PPQ.h"
#include "../../Utils/ChartTypes.h"

struct NoteData
{
    uint8_t velocity;

    NoteData() : velocity(0) {}
    NoteData(uint8_t vel) : velocity(vel) {}

    operator uint8_t() const { return velocity; }
    operator bool() const { return velocity > 0; }
};

using NoteStateMap = std::map<PPQ, NoteData>;
using NoteStateMapArray = std::array<NoteStateMap, 128>;

enum class Dynamic
{
    NONE = 0,
    GHOST = 1,
    ACCENT = 127,
};

struct MidiPitchDefinitions
{
    enum class Drums
    {
        LANE_2 = 127,
        LANE_1 = 126,
        // BRE_1 = 124,
        // BRE_2 = 123,
        // BRE_3 = 122,
        // BRE_4 = 121,
        // BRE_5 = 120,
        SP = 116,
        TOM_GREEN = 112,
        TOM_BLUE = 111,
        TOM_YELLOW = 110,
        // FLAM = 109,
        // PHRASE_2 = 106,
        // PHRASE_1 = 105,
        // SOLO = 103,
        // EXPERT_5 = 101,
        EXPERT_GREEN = 100,
        EXPERT_BLUE = 99,
        EXPERT_YELLOW = 98,
        EXPERT_RED = 97,
        EXPERT_KICK = 96,
        EXPERT_KICK_2X = 95,
        // HARD_5 = 89,
        HARD_GREEN = 88,
        HARD_BLUE = 87,
        HARD_YELLOW = 86,
        HARD_RED = 85,
        HARD_KICK = 84,
        // MEDIUM_5 = 77,
        MEDIUM_GREEN = 76,
        MEDIUM_BLUE = 75,
        MEDIUM_YELLOW = 74,
        MEDIUM_RED = 73,
        MEDIUM_KICK = 72,
        // EASY_5 = 65,
        EASY_GREEN = 64,
        EASY_BLUE = 63,
        EASY_YELLOW = 62,
        EASY_RED = 61,
        EASY_KICK = 60
    };

    // Elite Drums (PART ELITE_DRUMS). Lower octave = gem placement; each lower
    // difficulty is a fixed -24 from Expert (Expert 73-82, +72 pedal). MVP covers
    // gem placement (1x/2x kick + 8 hand lanes); the upper-octave modifiers
    // (flam, indifferent hat, disco) and the pedal (72) are deferred.
    enum class EliteDrums
    {
        SP = 104,   // Overdrive / Star Power / Unison (pan-difficulty)

        EXPERT_RCRASH = 82, EXPERT_RIDE = 81, EXPERT_TOM3 = 80, EXPERT_TOM2 = 79,
        EXPERT_TOM1 = 78,   EXPERT_LCRASH = 77, EXPERT_HIHAT = 76, EXPERT_SNARE = 75,
        EXPERT_KICK = 74,   EXPERT_KICK_2X = 73,

        HARD_RCRASH = 58, HARD_RIDE = 57, HARD_TOM3 = 56, HARD_TOM2 = 55,
        HARD_TOM1 = 54,   HARD_LCRASH = 53, HARD_HIHAT = 52, HARD_SNARE = 51,
        HARD_KICK = 50,   HARD_KICK_2X = 49,

        MEDIUM_RCRASH = 34, MEDIUM_RIDE = 33, MEDIUM_TOM3 = 32, MEDIUM_TOM2 = 31,
        MEDIUM_TOM1 = 30,   MEDIUM_LCRASH = 29, MEDIUM_HIHAT = 28, MEDIUM_SNARE = 27,
        MEDIUM_KICK = 26,   MEDIUM_KICK_2X = 25,

        EASY_RCRASH = 10, EASY_RIDE = 9, EASY_TOM3 = 8, EASY_TOM2 = 7,
        EASY_TOM1 = 6,    EASY_LCRASH = 5, EASY_HIHAT = 4, EASY_SNARE = 3,
        EASY_KICK = 2,    EASY_KICK_2X = 1,
    };

    enum class Guitar
    {
        LANE_2 = 127,
        LANE_1 = 126,
        // BRE_1 = 124,
        // BRE_2 = 123,
        // BRE_3 = 122,
        // BRE_4 = 121,
        // BRE_5 = 120,
        SP = 116,
        // PHRASE_2 = 106,
        // PHRASE_1 = 105,
        TAP = 104,
        // SOLO = 103,
        EXPERT_STRUM = 102,
        EXPERT_HOPO = 101,
        EXPERT_ORANGE = 100,
        EXPERT_BLUE = 99,
        EXPERT_YELLOW = 98,
        EXPERT_RED = 97,
        EXPERT_GREEN = 96,
        EXPERT_OPEN = 95,
        HARD_STRUM = 90,
        HARD_HOPO = 89,
        HARD_ORANGE = 88,
        HARD_BLUE = 87,
        HARD_YELLOW = 86,
        HARD_RED = 85,
        HARD_GREEN = 84,
        HARD_OPEN = 83,
        MEDIUM_STRUM = 78,
        MEDIUM_HOPO = 77,
        MEDIUM_ORANGE = 76,
        MEDIUM_BLUE = 75,
        MEDIUM_YELLOW = 74,
        MEDIUM_RED = 73,
        MEDIUM_GREEN = 72,
        MEDIUM_OPEN = 71,
        EASY_STRUM = 66,
        EASY_HOPO = 65,
        EASY_ORANGE = 64,
        EASY_BLUE = 63,
        EASY_YELLOW = 62,
        EASY_RED = 61,
        EASY_GREEN = 60,
        EASY_OPEN = 59
    };
};
