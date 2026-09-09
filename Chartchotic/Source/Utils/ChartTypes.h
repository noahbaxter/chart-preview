#pragma once

#include <JuceHeader.h>
#include "../Midi/Utils/PPQ.h"
#include "../UI/ControlConstants.h"

// Windows compatibility - uint is not defined by default on Windows
#if defined(_WIN32) || defined(_WIN64) || defined(__WINDOWS__) || defined(_MSC_VER)
    typedef unsigned int uint;
#endif

//==============================================================================
// CONSTANTS

constexpr uint LANE_COUNT = 7;  // Number of playable lanes (0-6)

//==============================================================================
// State helpers

inline Part getPartFromState(juce::ValueTree &state)
{
    return (Part)(int)state.getProperty("part");
}

inline RenderType getRenderTypeFromState(juce::ValueTree &state)
{
    return getRenderType(getPartFromState(state));
}

inline bool isPart(juce::ValueTree &state, Part part)
{
    return (int)state.getProperty("part") == (int)part;
}

constexpr int DRUM_KICK_COLUMN    = 0;
constexpr int DRUM_KICK_2X_COLUMN = 6;

inline bool isDrumKick(uint gemColumn)
{
    return gemColumn == DRUM_KICK_COLUMN || gemColumn == DRUM_KICK_2X_COLUMN;
}

inline uint drumColumnIndex(uint gemColumn)
{
    return (gemColumn == DRUM_KICK_2X_COLUMN) ? DRUM_KICK_COLUMN : gemColumn;
}

inline bool isBarNote(uint gemColumn, Part part)
{
    if (part == Part::GUITAR)
        return gemColumn == 0;
    else
        return isDrumKick(gemColumn);
}

//==============================================================================
// DRAWING — DrawOrder and DrawCallMap are in Visual/Utils/DrawingConstants.h
#include "../Visual/Utils/DrawingConstants.h"

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
    STEP,
};

// Tempo and time signature change event (used for REAPER tempo map queries)
struct TempoTimeSignatureEvent
{
    PPQ ppqPosition;
    double bpm;
    int timeSigNumerator;
    int timeSigDenominator;
    bool timeSigReset;
    int measurePos;            // 0-indexed measure number at this position (from host)
    double beatPos;            // Beat position within the measure (from host, in denominator units)

    TempoTimeSignatureEvent()
        : ppqPosition(0.0), bpm(120.0), timeSigNumerator(4), timeSigDenominator(4),
          timeSigReset(true), measurePos(0), beatPos(0.0) {}
    TempoTimeSignatureEvent(PPQ ppq, double tempo, int sigNum, int sigDenom,
                            bool sigReset = true, int mPos = 0, double bPos = 0.0)
        : ppqPosition(ppq), bpm(tempo), timeSigNumerator(sigNum), timeSigDenominator(sigDenom),
          timeSigReset(sigReset), measurePos(mPos), beatPos(bPos) {}
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