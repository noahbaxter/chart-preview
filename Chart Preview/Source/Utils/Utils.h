#pragma once

#include <JuceHeader.h>
#include "PPQ.h"

// Windows compatibility - uint is not defined by default on Windows
#if defined(_WIN32) || defined(_WIN64) || defined(__WINDOWS__) || defined(_MSC_VER)
    typedef unsigned int uint;
#endif

//==============================================================================
// CONSTANTS

constexpr uint LANE_COUNT = 7;  // Number of note lanes (0-6)
constexpr float OPACITY_FADE_START = 0.9f;  // Position where opacity starts fading

constexpr float SUSTAIN_WIDTH = 0.1f;
constexpr float SUSTAIN_OPACITY = 0.7f;

constexpr float GEM_SIZE = 0.9f; 
constexpr float BAR_SIZE = 0.9f;

// TODO: hook up
constexpr float LANE_WIDTH = 0.8f;
constexpr float LANE_OPACITY = 0.2f;

//==============================================================================
// MENUS

enum class Part { GUITAR = 1, DRUMS, REAL_DRUMS };
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
// State helpers

inline bool isPart(juce::ValueTree &state, Part part)
{
    return (int)state.getProperty("part") == (int)part;
}

//==============================================================================
// DRAWING

enum class DrawOrder
{
    BACKGROUND,
    TRACK,
    GRID,
    LANE,
    BAR,
    SUSTAIN,
    NOTE,
    OVERLAY
};

using DrawCallMap = std::map<DrawOrder, std::vector<std::function<void(juce::Graphics&)>>>;

//==============================================================================
// CHART EVENTS

enum class Gem
{
    NONE,
    HOPO_GHOST,
    NOTE,
    TAP_ACCENT,
    CYM_GHOST,
    CYM,
    CYM_ACCENT,
};

enum class Dynamic
{
    NONE=0,
    GHOST=1,
    ACCENT=127,
};

enum class Gridline
{
    MEASURE,
    BEAT,
    HALF_BEAT,
};

enum class SustainType
{
    SUSTAIN,
    LANE,
    SOLO,
    BRE
};

struct SustainEvent
{
    PPQ startPPQ;
    PPQ endPPQ;
    uint gemColumn;
    SustainType sustainType;
    Gem gemType;
};

//==============================================================================
// TYPES

using NoteStateMap = std::map<PPQ, uint8_t>;
using NoteStateMapArray = std::array<NoteStateMap, 128>;
using TrackFrame = std::array<Gem, LANE_COUNT>; // All the simultaneous notes at a moment in time
using TrackWindow = std::map<PPQ, TrackFrame>;  // All the frames in the track window
using SustainWindow = std::vector<SustainEvent>; // Active sustains in the track window
using GridlineMap = std::map<PPQ, Gridline>;

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