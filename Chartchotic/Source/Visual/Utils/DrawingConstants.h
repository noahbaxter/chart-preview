/*
    ==============================================================================

        DrawingConstants.h
        Author: Noah Baxter

        Central location for drawing and rendering constants used throughout
        the visualization pipeline. Includes opacity values, visual effects,
        and rendering parameters.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <map>
#include <vector>
#include <functional>
#include "BemaniConfig.h"

// Windows compatibility
#if defined(_WIN32) || defined(_WIN64) || defined(__WINDOWS__) || defined(_MSC_VER)
    typedef unsigned int uint;
#endif

//==============================================================================
// Opacity & Visual Effects
//==============================================================================

// Gem and note opacity
constexpr float SUSTAIN_OPACITY = 0.7f;      // Sustain note opacity
constexpr float LANE_OPACITY = 0.4f;         // Lane sustain opacity

// Gridline opacity by type
constexpr float MEASURE_OPACITY = 1.0f;      // Gridline opacity for measure lines
constexpr float BEAT_OPACITY = 0.3f;         // Gridline opacity for beat lines
constexpr float HALF_BEAT_OPACITY = 0.15f;   // Gridline opacity for half-beat lines

// Write-mode gridline opacities. The marker PNGs already bake in alpha
// (MEASURE peaks ~191/255, BEAT/HALF_BEAT peak ~128/255), which compresses
// the per-type contrast. We pick multipliers that, AFTER the baked alpha,
// keep MEASURE clearly brightest, BEAT clearly the rhythm anchor, and STEP
// the dimmest "subgrid". HALF_BEAT is hidden in write mode (renderer skips it).
// STEP is generated only in write mode.
constexpr float WRITE_MEASURE_OPACITY   = 1.0f;   // baked-in 0.75 -> ~0.75 effective
constexpr float WRITE_BEAT_OPACITY      = 0.9f;   // baked-in 0.50 -> ~0.45 effective
constexpr float WRITE_HALF_BEAT_OPACITY = 0.35f;  // hidden in write mode (kept for completeness)
constexpr float WRITE_STEP_OPACITY      = 0.25f;  // baked-in 0.50 -> ~0.13 effective (faded with depth)

// Side-of-highway protrusions: flat white tabs at write-mode MEASURE/BEAT
// gridlines, sticking horizontally outward from each highway edge. STEP
// gets no protrusion. The protrusion is THE structural anchor indicator —
// the on-highway lines stay subtle (same look as the placement grid).
//
// All sizes are fractions of strike-reference highway width — they scale
// with the highway like gems/glyphs do, NOT with absolute window pixels.
// Multiplied by widthRatio at render time for perspective foreshortening.
constexpr float WRITE_PROTRUSION_MEASURE_LENGTH_FRAC    = 0.080f;
constexpr float WRITE_PROTRUSION_BEAT_LENGTH_FRAC       = 0.045f;
constexpr float WRITE_PROTRUSION_MEASURE_THICKNESS_FRAC = 0.008f;
constexpr float WRITE_PROTRUSION_BEAT_THICKNESS_FRAC    = 0.008f;
// Small downward (closer-to-camera) Y nudge — protrusions are flat 2D, so
// they draw at the gridline's projected centerY, but the marker PNG's
// visible alpha line sits slightly below sprite center on the highway
// plane. This nudges the tabs to visually meet the line as drawn.
constexpr float WRITE_PROTRUSION_Z_NUDGE_FRAC           = 0.006f;

// Right-side measure number labels in write mode. Font size and gap are
// fractions of strike-reference highway width (scale with the highway
// the same way gems do). Anchored past the MEASURE protrusion's tip so
// the label sits clear of the white tab. Left-aligned so digits read
// consistently. REAPER-style 1-indexed display.
constexpr float WRITE_MEASURE_LABEL_FONT_FRAC      = 0.075f;
constexpr float WRITE_MEASURE_LABEL_GAP_FRAC       = 0.030f;
constexpr float WRITE_MEASURE_LABEL_MIN_FONT_PX    = 9.0f;   // screen-pixel readability floor — kept absolute

// Single visibility knob: approximate maximum number of labels visible
// across the rendered highway at once.
//
// Higher = more labels (denser, sub-divisions fill in). Lower = fewer
// (sparser, only structural beats). Internally converted to a normalized-
// position spacing threshold (1.0 / count) — labels are linear in TIME /
// scroll speed, so faster scroll naturally fits fewer measures-of-music
// per highway and finer divisions drop out, while the back of the
// highway can't stack regardless of perspective compression.
//
// Priority order when filtering: MEASURE > BEAT > HALF_BEAT > STEP.
constexpr float WRITE_LABEL_TARGET_COUNT = 3.0f;

// Highway texture
constexpr float TEXTURE_OPACITY_DEFAULT = 1.0f;  // Default highway texture opacity

// Hit animation rendering
constexpr float HIT_FLASH_OPACITY = 0.8f;    // Hit flash frame opacity at strikeline
constexpr float HIT_FLARE_OPACITY = 0.6f;    // Colored flare overlay opacity
constexpr double HIT_ANIMATION_DURATION = 0.10;   // Hit flash total duration (seconds)
constexpr double KICK_ANIMATION_DURATION = 0.15;  // Kick/bar flash total duration (seconds)
constexpr int HIT_FLARE_MAX_FRAME = 2;            // Show flare for first N frames of hit animation

// Far-end fade (highway length)
constexpr float FAR_FADE_DEFAULT = 1.20f;     // Default user slider value (position space)
constexpr float FAR_FADE_MIN     = 0.50f;     // Minimum user slider value
constexpr float FAR_FADE_MAX     = 5.00f;     // Maximum user slider value
constexpr float FAR_FADE_LEN     = 0.35f;     // Length of fade zone
constexpr float FAR_FADE_CURVE   = 1.0f;      // Fade exponent (1=linear)

inline float getHwyScale(bool isDrums) { return bemaniConfig.hwyScale(isDrums); }


inline float calculateFarFade(float position, float fadeEnd, float fadeLen, float fadeCurve)
{
    float fadeStart = fadeEnd - fadeLen;
    if (position <= fadeStart) return 1.0f;
    if (position >= fadeEnd)   return 0.0f;
    float t = (position - fadeStart) / fadeLen;
    return 1.0f - std::pow(t, fadeCurve);
}

//==============================================================================
// Draw Order & Render Pipeline
//==============================================================================

enum class DrawOrder
{
    BACKGROUND,
    TRACK,
    GRID,
    LANE,
    TRACK_SIDEBARS,
    TRACK_LANE_LINES,
    TRACK_STRIKELINE,
    SUSTAIN,
    BAR,
    NOTE,
    TRACK_CONNECTORS,
    OVERLAY,
    BAR_ANIMATION,
    NOTE_ANIMATION,
    TEXT_EVENT,
};

static constexpr int DRAW_ORDER_COUNT = 15;

//==============================================================================
// Text Event Data
//==============================================================================

struct TimeBasedFlipRegion {
    double startTime;
    double endTime;
};
using TimeBasedFlipRegions = std::vector<TimeBasedFlipRegion>;

struct TimeBasedEventMarker {
    double time;
    juce::String label;
};
using TimeBasedEventMarkers = std::vector<TimeBasedEventMarker>;
static constexpr int MAX_DRAW_COLUMNS = 8;
using DrawCallBucket = std::vector<std::function<void(juce::Graphics&)>>;
using DrawCallGrid = std::array<std::array<DrawCallBucket, MAX_DRAW_COLUMNS>, DRAW_ORDER_COUNT>;
using DrawCallMap = DrawCallGrid;
