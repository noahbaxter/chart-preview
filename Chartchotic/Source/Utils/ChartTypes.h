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

constexpr uint LANE_COUNT = 12;  // Max playable columns: 4-lane uses 0-6; elite uses kick(0) + 8 hand lanes(1-8) + kick2x(9) + stomp(10) + splash(11)

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

constexpr int DRUM_KICK_COLUMN     = 0;
constexpr int DRUM_KICK_2X_COLUMN  = 6;   // 4-lane: 2x kick shares the kick lane
constexpr int ELITE_KICK_2X_COLUMN = 9;   // elite: col 6 is a real hand lane (Tom 3), so 2x kick moves to a virtual column
constexpr int ELITE_HIHAT_COLUMN   = 2;   // elite: the yellow cymbal lane, the only lane with open/closed/indifferent state

// `part` is deliberately NOT defaulted. A default of Part::DRUMS silently answers the
// 4-lane question for elite, where column 6 is Tom 3 rather than the 2x kick, and every
// caller that forgot it became a bug (kick split, sustain widths, marquee rects, note
// pitches). Making it explicit means the compiler finds the next one.
inline bool isDrumKick(uint gemColumn, Part part)
{
    if (part == Part::ELITE_DRUMS)
        return gemColumn == DRUM_KICK_COLUMN || gemColumn == ELITE_KICK_2X_COLUMN;
    return gemColumn == DRUM_KICK_COLUMN || gemColumn == DRUM_KICK_2X_COLUMN;
}

// The 2x kick's own column, whichever one this part puts it on.
inline bool isDrum2xKick(uint gemColumn, Part part)
{
    return isDrumKick(gemColumn, part) && gemColumn != (uint)DRUM_KICK_COLUMN;
}

inline uint drumColumnIndex(uint gemColumn, Part part)
{
    if (part == Part::ELITE_DRUMS)
        return (gemColumn == ELITE_KICK_2X_COLUMN) ? DRUM_KICK_COLUMN : gemColumn;
    return (gemColumn == DRUM_KICK_2X_COLUMN) ? DRUM_KICK_COLUMN : gemColumn;
}

// Elite hand lanes that are cymbals (Hi-Hat, L-Crash, Ride, R-Crash); the rest
// (Snare, Toms) are drums. Each lane is drum XOR cymbal, fixed by lane, so this is
// the chart-side source for gem type (mirrors ELITE_LANE_STYLES.cymbal on the visual side).
inline bool isEliteCymbalLane(uint gemColumn)
{
    return gemColumn == 2 || gemColumn == 3 || gemColumn == 7 || gemColumn == 8;
}

// Whether authoring on `lane` writes a cymbal. Elite is fixed by lane and ignores the Cym
// toggle; 4-lane/5-lane put cymbals on 2..4 behind it. The write path MUST agree with what
// TrackResolver decides on parse, or the ghost preview draws with gemZ while the note it
// places lands on cymZ. AuthoringConfig::hasCymbalToggle is the same fact from the other
// direction: no toggle means the lane decides.
inline bool authorsCymbal(uint lane, Part part, bool cymbalToggle)
{
    if (part == Part::ELITE_DRUMS) return isEliteCymbalLane(lane);
    return lane >= 2 && lane <= 4 && cymbalToggle;
}

// The elite Hi-Hat lane (yellow cymbal) — the only lane that carries Open/Closed/Indifferent
// pedal state (spec: Yellow defaults to Open, coincident Pedal Down = Closed, coincident
// Indifferent marker = Indifferent).
inline bool isEliteHiHatLane(uint gemColumn)
{
    return gemColumn == (uint)ELITE_HIHAT_COLUMN;
}

inline bool isBarNote(uint gemColumn, Part part)
{
    if (part == Part::GUITAR)
        return gemColumn == 0;
    else
        return isDrumKick(gemColumn, part);
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
    STOMP,          // Elite hi-hat pedal: closed-pedal "stomp" (mini kick bar over the hi-hat zone)
    SPLASH,         // Elite hi-hat pedal: "splash" (same bar; also a hi-hat sustain generator)
};

// Elite hi-hat pedal state, orthogonal to the gem's ghost/accent type. Only meaningful on the
// Hi-Hat (yellow cymbal) lane. Open is the spec default; Closed needs a coincident pedal-down;
// Indifferent renders as an ordinary cymbal.
enum class HiHatState
{
    None,
    Open,
    Closed,
    Indifferent,
};

struct GemWrapper
{
    Gem gem;
    bool starPower;
    HiHatState hihat;
    // Elite flam: a grace note ahead of the gem. Orthogonal to gem type, to dynamics and to
    // hat state (an "Open Ghost Flam" is legal), so it rides alongside them rather than
    // doubling the Gem enum. Never set on kicks — those flam as stacked 1x + 2x.
    bool flam;

    GemWrapper() : gem(Gem::NONE), starPower(false), hihat(HiHatState::None), flam(false) {}
    GemWrapper(Gem g, bool sp = false, HiHatState hh = HiHatState::None, bool fl = false)
        : gem(g), starPower(sp), hihat(hh), flam(fl) {}
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