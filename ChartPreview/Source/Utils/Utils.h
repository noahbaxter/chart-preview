#pragma once

#include <JuceHeader.h>
#include "PPQ.h"

// Windows compatibility - uint is not defined by default on Windows
#if defined(_WIN32) || defined(_WIN64) || defined(__WINDOWS__) || defined(_MSC_VER)
    typedef unsigned int uint;
#endif

//==============================================================================
// CONSTANTS

constexpr uint LANE_COUNT = 7;  // Number of playable lanes (0-6)

//==============================================================================
// MENUS

enum class Part { GUITAR = 1, DRUMS, REAL_DRUMS };
// enum class GuitarType { LEAD = 1, RHYTHM, BASS };
enum class DrumType { NORMAL = 1, PRO };
enum class SkillLevel { EASY = 1, MEDIUM, HARD, EXPERT };
enum class ViewToggle { STAR_POWER = 1, KICK_2X, DYNAMICS };
enum class HopoMode { OFF = 1, SIXTEENTH, DOT_SIXTEENTH, CLASSIC_170, EIGHTH };

const juce::StringArray partLabels = {"Guitar", "Drums"};
// const juce::StringArray guitarTypeLabels = {"Lead", "Rhythm", "Bass"};
const juce::StringArray drumTypeLabels = {"Normal", "Pro"};
const juce::StringArray skillLevelLabels = {"Easy", "Medium", "Hard", "Expert"};
const juce::StringArray viewToggleLabels = {"Star Power", "Kick 2x", "Dynamics"};
const juce::StringArray hopoModeLabels = {"Off", "16th", "Dot 16th", "170 Tick", "8th"};

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

using DrawCallMap = std::map<DrawOrder, std::map<uint, std::vector<std::function<void(juce::Graphics&)>>>>;

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

struct GemWrapper
{
    Gem gem;
    bool starPower;

    GemWrapper() : gem(Gem::NONE), starPower(false) {}
    GemWrapper(Gem g, bool sp = false) : gem(g), starPower(sp) {}
};

enum class Gridline
{
    MEASURE,
    BEAT,
    HALF_BEAT,
};

// Tempo and time signature change event (used for REAPER tempo map queries)
struct TempoTimeSignatureEvent
{
    PPQ ppqPosition;           // Musical position (beats)
    double bpm;                // Tempo in beats per minute
    int timeSigNumerator;      // Time signature numerator (e.g., 4 in 4/4)
    int timeSigDenominator;    // Time signature denominator (e.g., 4 in 4/4)
    bool timeSigReset;         // True if this event explicitly changed the time signature (reset measure anchor). False if carried forward from previous.

    TempoTimeSignatureEvent()
        : ppqPosition(0.0), bpm(120.0), timeSigNumerator(4), timeSigDenominator(4), timeSigReset(true) {}
    TempoTimeSignatureEvent(PPQ ppq, double tempo, int sigNum, int sigDenom, bool sigReset = true)
        : ppqPosition(ppq), bpm(tempo), timeSigNumerator(sigNum), timeSigDenominator(sigDenom), timeSigReset(sigReset) {}
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
    GemWrapper gemType;
};

//==============================================================================
// TYPES

using TrackFrame = std::array<GemWrapper, LANE_COUNT>;
using TrackWindow = std::map<PPQ, TrackFrame>;
using SustainWindow = std::vector<SustainEvent>;
using GridlineMap = std::map<PPQ, Gridline>;