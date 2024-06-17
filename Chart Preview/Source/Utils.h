#pragma once

#include <JuceHeader.h>

//==============================================================================
// MENUS

enum class Part { GUITAR = 1, DRUMS };
// enum class GuitarType { LEAD = 1, RHYTHM, BASS };
enum class DrumType { NORMAL = 1, PRO };
enum class SkillLevel { EASY = 1, MEDIUM, HARD, EXPERT };
enum class ViewToggle { STAR_POWER = 1, KICK_2X, DYNAMICS };

const juce::StringArray partLabels = {"Guitar", "Drums"};
// const juce::StringArray guitarTypeLabels = {"Lead", "Rhythm", "Bass"};
const juce::StringArray drumTypeLabels = {"Normal", "Pro"};
const juce::StringArray skillLevelLabels = {"Easy", "Medium", "Hard", "Expert"};
const juce::StringArray viewToggleLabels = {"Star Power", "Kick 2x", "Dynamics"};

//==============================================================================
// MIDI MAPPINGS

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
        // TAP = 104,
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

//==============================================================================
// CHART EVENTS

enum class ChartEvents
{
    NOTE_GREEN,
    NOTE_RED,
    NOTE_YELLOW,
    NOTE_BLUE,
    NOTE_ORANGE,
    NOTE_OPEN,

    NOTE_GREEN_OFF,
    NOTE_RED_OFF,
    NOTE_YELLOW_OFF,
    NOTE_BLUE_OFF,
    NOTE_ORANGE_OFF,
    NOTE_OPEN_OFF,

    NOTE_HOPO_GREEN,
    NOTE_HOPO_RED,
    NOTE_HOPO_YELLOW,
    NOTE_HOPO_BLUE,
    NOTE_HOPO_ORANGE,
    NOTE_HOPO_OPEN,

    NOTE_TAP_GREEN,
    NOTE_TAP_RED,
    NOTE_TAP_YELLOW,
    NOTE_TAP_BLUE,
    NOTE_TAP_ORANGE,

    NOTE_KICK,
    NOTE_KICK_2X,
    NOTE_CYM_YELLOW,
    NOTE_CYM_BLUE,
    NOTE_CYM_GREEN,

    NOTE_RED_GHOST,
    NOTE_YELLOW_GHOST,
    NOTE_BLUE_GHOST,
    NOTE_GREEN_GHOST,
    NOTE_CYM_YELLOW_GHOST,
    NOTE_CYM_BLUE_GHOST,
    NOTE_CYM_GREEN_GHOST,

    NOTE_RED_ACCENT,
    NOTE_YELLOW_ACCENT,
    NOTE_BLUE_ACCENT,
    NOTE_GREEN_ACCENT,
    NOTE_CYM_YELLOW_ACCENT,
    NOTE_CYM_BLUE_ACCENT,
    NOTE_CYM_GREEN_ACCENT,

    SP_START,
    SP_END,
    LANE_1_START,
    LANE_1_END,
    LANE_2_START,
    LANE_2_END,
};
