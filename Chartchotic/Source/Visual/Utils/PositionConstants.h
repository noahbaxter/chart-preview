/*
    ==============================================================================

        PositionConstants.h
        Created by Claude Code (refactoring positioning logic)
        Author: Noah Baxter

        This file contains positioning constants and coordinate lookup tables.
        All mathematical calculations are in PositionMath.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

// Windows compatibility
#if defined(_WIN32) || defined(_WIN64) || defined(__WINDOWS__) || defined(_MSC_VER)
    typedef unsigned int uint;
#endif

namespace PositionConstants
{
    // Lane coordinate system for sustain rendering
    struct LaneCorners
    {
        float leftX, rightX, centerY;
    };

    // Normalized coordinate data for positioning elements
    // x,y
    // 0,0 ----- 1,0
    // |          |
    // |          |
    // |          |
    // 0,1 ----- 1,1

    // x2, y2, w2
    // | | | | | |  FAR END
    // | | | | | |
    // | | | | | |
    // | | | | | |
    // | | | | | |
    // v v v v v v  STRIKELINE
    // x1, y1, w1
    
    struct NormalizedCoordinates
    {
        float normX1, normX2;
        float normY1, normY2;
        float normWidth1, normWidth2;
    };

    struct CoordinateOffset
    {
        float xOffset;      // X offset in pixels from glyph center
        float yOffset;      // Y offset in pixels from glyph center
        float widthScale;   // Width multiplier
        float heightScale;  // Height multiplier
    };

    // 3D perspective calculation parameters
    struct PerspectiveParams
    {
        float highwayDepth;
        float playerDistance;
        float perspectiveStrength;
        float exponentialCurve;
        float vanishingPointDepth;   // Depth at which progress=0 (normalizes perspective across full highway)
        float vanishingPointY;       // Y offset for vanishing point (added to normY2; negative = higher on screen)
        float nearWidth;             // Near-end width multiplier (controls bottom spread / edge angle)
        float xOffsetMultiplier;
        float barNoteHeightRatio;
        float regularNoteHeightRatio;
    };

    //==============================================================================
    // Per-instrument offset bundle (Z offsets at REFERENCE_HEIGHT + strike positions)
    struct InstrumentOffsets
    {
        float gridZ;
        float gemZ;     // Toms / regular strum notes
        float cymZ;     // Cymbals (drums) — tunable independently from toms
        float barZ;
        float hitGemZ;
        float hitBarZ;
        float strikePosGem;
        float strikePosBar;
    };

    // Element width/height scale pair
    struct ElementScale
    {
        float width  = 1.0f;
        float height = 1.0f;
    };

    // Hit animation scale (uniform + per-axis, same pattern as OverlayAdjust)
    struct HitScale
    {
        float scale  = 1.0f;   // Uniform multiplier (both axes)
        float width  = 1.0f;   // Per-axis W
        float height = 1.0f;   // Per-axis H
    };

    //==============================================================================
    // Size & Scale Factors
    constexpr float GEM_SIZE = 1.252f;                   // Regular gem/note scaling factor (1.089 × 1.15 — the 1.15 is a baked default size bump; user-facing scales sit on top of this)
    constexpr float BAR_SIZE = 1.026f;                  // Bar note (kick/open) scaling factor
    constexpr float GRIDLINE_WIDTH_SCALE = 1.12f;       // Gridline width relative to fretboard
    constexpr float TEXT_EVENT_WIDTH_SCALE = 1.00f;    // Text event marker width relative to fretboard
    constexpr float GRIDLINE_POS_OFFSET = -0.020f;      // Nudge gridlines forward in position space
    constexpr float BAR_FRETBOARD_FIT = 1.0f;            // Bar note base scale to fill fretboard polygon
    constexpr float SUSTAIN_WIDTH = 0.15f;              // Sustain width multiplier
    constexpr float SUSTAIN_OPEN_WIDTH = 0.7f;          // Open sustain width multiplier (narrower)
    inline float LANE_WIDTH = 1.0f;                       // Lane width multiplier (mutable for debug tuning)
    inline float LANE_OPEN_WIDTH = 0.9f;                // Open lane width multiplier (mutable for debug tuning)

    //==============================================================================
    // Sustain Geometry Constants
    constexpr float SUSTAIN_WIDTH_MULTIPLIER = 0.8f;    // Sustain slightly narrower than gems
    constexpr float SUSTAIN_CAP_RADIUS_SCALE = 0.25f;   // Scale for rounded sustain caps
    constexpr float SUSTAIN_MARGIN_SCALE = 0.1f;        // Small margin above/below center

    //==============================================================================
    // Curved Sustain & Lane Constants
    // Curve values: positive = convex bow outward, negative = concave bow inward
    constexpr float SUSTAIN_START_CURVE = 0.015f;       // Near edge (strikeline side)
    constexpr float SUSTAIN_END_CURVE = -0.010f;        // Far edge
    constexpr float BAR_SUSTAIN_START_CURVE = -0.015f;  // Bar (open/kick) near edge
    constexpr float BAR_SUSTAIN_END_CURVE = -0.015f;    // Bar (open/kick) far edge

    constexpr float SUSTAIN_START_OFFSET = 0.0f;        // Position nudge for sustain start
    constexpr float SUSTAIN_END_OFFSET = -0.05f;        // Sustain ends slightly before note
    constexpr float BAR_SUSTAIN_START_OFFSET = 0.0f;
    constexpr float BAR_SUSTAIN_END_OFFSET = -0.05f;

    constexpr float SUSTAIN_CLIP = -0.015f;             // How far past strikeline before clamping
    constexpr float BAR_SUSTAIN_CLIP = -0.015f;

    // Lane shape config (tuneable bundle)
    struct LaneShapeConfig
    {
        float startOffset    = -0.010f;  // Z position nudge at start (before note)
        float endOffset      = -0.010f;  // Z position nudge at end (after note)
        float innerStartArc  =  0.040f;  // Lane-local arc at start edge
        float innerEndArc    = -0.040f;  // Lane-local arc at end edge
        float outerStartArc  = -0.025f;  // Fretboard-wide parabola at start
        float outerEndArc    = -0.035f;  // Fretboard-wide parabola at end
    };
    constexpr LaneShapeConfig LANE_SHAPE_DEFAULT = {};

    //==============================================================================
    // Overlay adjustment params (per overlay type)
    struct OverlayAdjust
    {
        float offsetX = 0.0f;   // X offset (fraction of width)
        float offsetY = 0.0f;   // Y offset (fraction of height)
        float scaleX  = 1.0f;   // Horizontal scale
        float scaleY  = 1.0f;   // Vertical scale
        float scale   = 1.0f;   // Uniform scale (stacks with scaleX/scaleY)
    };

    enum OverlayType
    {
        OVERLAY_GUITAR_TAP = 0,
        OVERLAY_DRUM_NOTE_GHOST,
        OVERLAY_DRUM_NOTE_ACCENT,
        OVERLAY_DRUM_CYM_GHOST,
        OVERLAY_DRUM_CYM_ACCENT,
        NUM_OVERLAY_TYPES
    };

    constexpr OverlayAdjust OVERLAY_DEFAULTS[NUM_OVERLAY_TYPES] = {
        { 0.0f, -0.03f, 1.0f, 1.0f, 1.0f },  // GUITAR_TAP
        { 0.0f, -0.03f, 1.0f, 1.0f, 1.0f },  // DRUM_NOTE_GHOST
        { 0.0f, -0.10f, 1.0f, 1.0f, 1.0f },  // DRUM_NOTE_ACCENT
        { 0.0f, -0.04f, 1.0f, 1.0f, 1.33f }, // DRUM_CYM_GHOST
        { 0.0f, -0.08f, 1.0f, 1.0f, 1.0f },  // DRUM_CYM_ACCENT
    };

    //==============================================================================
    // Lane Counts
    constexpr size_t GUITAR_LANE_COUNT = 6;             // Open + 5 frets
    constexpr size_t DRUM_LANE_COUNT = 5;               // Kick + 4 pads
    constexpr size_t ELITE_DRUM_LANE_COUNT = 9;         // Kick + 8 hand lanes (Snare-Hat-LCrash-Tom1-Tom2-Tom3-Ride-RCrash)

    //==============================================================================
    // Animation Positioning & Scaling Factors

    constexpr CoordinateOffset GUITAR_ANIMATION_OFFSETS[] = {
        {0.0f, 0.0f, 1.4f, 8.0f},   // Bar 0 - Open note
        {0.0f, 0.0f, 1.6f, 3.5f},   // Col 1 - Green
        {0.0f, 0.0f, 1.6f, 3.5f},   // Col 2 - Red
        {0.0f, 0.0f, 1.6f, 3.5f},   // Col 3 - Yellow
        {0.0f, 0.0f, 1.6f, 3.5f},   // Col 4 - Blue
        {0.0f, 0.0f, 1.6f, 3.5f}    // Col 5 - Orange
    };

    constexpr CoordinateOffset DRUM_ANIMATION_OFFSETS[] = {
        {0.0f, -8.0f, 1.4f, 10.0f}, // Bar 0 - Kick/2x Kick
        {0.0f, 0.0f, 1.6f, 3.5f},   // Col 1 - Red
        {0.0f, 0.0f, 1.6f, 3.5f},   // Col 2 - Yellow
        {0.0f, 0.0f, 1.6f, 3.5f},   // Col 3 - Blue
        {0.0f, 0.0f, 1.6f, 3.5f}    // Col 4 - Green
    };

    // Elite drums: kick bar + 8 hand lanes (same anim shape as drums)
    constexpr CoordinateOffset ELITE_DRUM_ANIMATION_OFFSETS[] = {
        {0.0f, -8.0f, 1.4f, 10.0f}, // 0 Kick
        {0.0f, 0.0f, 1.6f, 3.5f},   // 1 Snare
        {0.0f, 0.0f, 1.6f, 3.5f},   // 2 Hi-Hat
        {0.0f, 0.0f, 1.6f, 3.5f},   // 3 Left Crash
        {0.0f, 0.0f, 1.6f, 3.5f},   // 4 Tom 1
        {0.0f, 0.0f, 1.6f, 3.5f},   // 5 Tom 2
        {0.0f, 0.0f, 1.6f, 3.5f},   // 6 Tom 3
        {0.0f, 0.0f, 1.6f, 3.5f},   // 7 Ride
        {0.0f, 0.0f, 1.6f, 3.5f}    // 8 Right Crash
    };

    //==============================================================================
    // Fretboard Boundary Coordinates (for bezier positioning system)
    constexpr NormalizedCoordinates guitarFretboardCoords =
        {0.16f, 0.34f, 0.73f, 0.234f, 0.68f, 0.32f};
    constexpr NormalizedCoordinates drumFretboardCoords =
        {0.16f, 0.34f, 0.735f, 0.239f, 0.68f, 0.32f};
    // Elite drums reuse the drum board shape for now (getFretboardEdge is not yet
    // config-aware, so this MUST match drumFretboardCoords or lanes/fill diverge).
    constexpr NormalizedCoordinates eliteDrumFretboardCoords =
        {0.16f, 0.34f, 0.735f, 0.239f, 0.68f, 0.32f};
    constexpr float FRETBOARD_SCALE = 1.25f;

    //==============================================================================
    // Highway Range (bezier system defaults)
    constexpr float HIGHWAY_POS_START = -0.3f;
    constexpr float HIGHWAY_POS_END = 1.12f;

    //==============================================================================
    // Bezier Lane Coordinate Tables (sustain/lane rendering, slightly different from glyph tables)
    constexpr NormalizedCoordinates guitarBezierLaneCoords[] = {
        {0.208f, 0.350f, 0.73f, 0.234f, 0.582f, 0.300f},     // Open
        {0.234f, 0.367f, 0.71f, 0.22f, 0.098f, 0.056f},      // Green
        {0.342f, 0.424f, 0.71f, 0.22f, 0.099f, 0.052f},      // Red
        {0.451f, 0.474f, 0.71f, 0.22f, 0.099f, 0.052f},      // Yellow
        {0.559f, 0.524f, 0.71f, 0.22f, 0.100f, 0.052f},      // Blue
        {0.668f, 0.576f, 0.71f, 0.22f, 0.098f, 0.056f}       // Orange
    };
    constexpr NormalizedCoordinates drumBezierLaneCoords[] = {
        {0.212f, 0.354f, 0.735f, 0.239f, 0.574f, 0.290f},    // Kick (1x and 2x)
        {0.234f, 0.376f, 0.70f, 0.22f, 0.130f, 0.060f},      // Red
        {0.376f, 0.436f, 0.70f, 0.22f, 0.120f, 0.064f},      // Yellow
        {0.506f, 0.500f, 0.70f, 0.22f, 0.118f, 0.066f},      // Blue
        {0.636f, 0.564f, 0.70f, 0.22f, 0.124f, 0.060f}       // Green
    };
    // Elite drums: kick (full width) + 8 hand lanes evenly spaced across the same
    // fretboard span the 4 drum pads use (near 0.234..0.760, far 0.376..0.624).
    // Starting values; tune live via the render harness.
    constexpr NormalizedCoordinates eliteDrumBezierLaneCoords[] = {
        {0.212f, 0.354f, 0.735f, 0.239f, 0.574f, 0.290f},    // 0 Kick (full width)
        {0.2340f, 0.376f, 0.70f, 0.22f, 0.059f, 0.028f},     // 1 Snare
        {0.2998f, 0.407f, 0.70f, 0.22f, 0.059f, 0.028f},     // 2 Hi-Hat
        {0.3655f, 0.438f, 0.70f, 0.22f, 0.059f, 0.028f},     // 3 Left Crash
        {0.4313f, 0.469f, 0.70f, 0.22f, 0.059f, 0.028f},     // 4 Tom 1
        {0.4970f, 0.500f, 0.70f, 0.22f, 0.059f, 0.028f},     // 5 Tom 2
        {0.5628f, 0.531f, 0.70f, 0.22f, 0.059f, 0.028f},     // 6 Tom 3
        {0.6285f, 0.562f, 0.70f, 0.22f, 0.059f, 0.028f},     // 7 Ride
        {0.6943f, 0.593f, 0.70f, 0.22f, 0.059f, 0.028f}      // 8 Right Crash
    };

    //==============================================================================
    // Curved Note Rendering (pre-baked image cache)
    constexpr float NOTE_CURVATURE = -0.02f;        // Arc height as fraction of fretboard width (guitar)
    constexpr float NOTE_CURVATURE_DRUMS = -0.016f; // Drum default (slightly less bow than guitar)
    constexpr float BAR_CURVATURE = 0.0f;          // Bars stay flat (span full fretboard)
    constexpr int NOTE_CACHE_DOWNSAMPLE = 2;       // Source resolution divisor (2 = 1/2 res)

    //==============================================================================
    // Per-instrument Z offsets (reference pixels at 720px height, positive = down)
    // Scaled by (height / REFERENCE_HEIGHT) at render time for resolution independence.
    constexpr float REFERENCE_HEIGHT = 720.0f;

    constexpr InstrumentOffsets GUITAR_OFFSETS = {
        0.0f,       // gridZ
        4.0f,       // gemZ
        0.0f,       // cymZ (unused — guitars have no cymbals)
        16.0f,      // barZ
        14.0f,      // hitGemZ
        9.0f,       // hitBarZ
        0.0f,       // strikePosGem
        0.0f        // strikePosBar
    };
    constexpr InstrumentOffsets DRUM_OFFSETS = {
        -20.0f,     // gridZ
        -12.0f,     // gemZ (toms)
        0.0f,       // cymZ (cymbals)
        0.0f,       // barZ
        6.0f,       // hitGemZ
        10.0f,      // hitBarZ
        0.0f,       // strikePosGem
        0.0f        // strikePosBar
    };

    // Per-column adjustment bundle (offsets in pixels at REFERENCE_HEIGHT, scales are multipliers)
    struct ColumnAdjust
    {
        float z      = 0.0f;  // Z pixel offset (vertical nudge)
        float sNear  = 1.0f;  // Uniform scale at strikeline (interpolated)
        float sFar   = 1.0f;  // Uniform scale at far end (interpolated)
        float w      = 1.0f;  // Width multiplier
        float h      = 1.0f;  // Height multiplier
    };

    constexpr ColumnAdjust GUITAR_COL_ADJUST[6] = {
        {0, 1, 1, 1, 1},                // Open
        {0, 1.05f, 1, 1, 1},            // Green
        {0, 1.1f, 1, 1, 1},             // Red
        {0, 1.1f, 1, 1, 1},             // Yellow
        {0, 1.1f, 1, 1, 1},             // Blue
        {0, 1.05f, 1, 1, 1}             // Orange
    };
    constexpr ColumnAdjust DRUM_COL_ADJUST[5] = {
        {0, 1, 1, 1, 1},                // Kick
        {0, 1, 1, 1, 1},                // Red
        {0, 1, 1, 1, 1},                // Yellow
        {0, 1, 1, 1, 1},                // Blue
        {0, 1, 1, 1, 1}                 // Green
    };
    constexpr ColumnAdjust ELITE_DRUM_COL_ADJUST[9] = {
        {0, 1, 1, 1, 1},                // 0 Kick
        {0, 1, 1, 1, 1},                // 1 Snare
        {0, 1, 1, 1, 1},                // 2 Hi-Hat
        {0, 1, 1, 1, 1},                // 3 Left Crash
        {0, 1, 1, 1, 1},                // 4 Tom 1
        {0, 1, 1, 1, 1},                // 5 Tom 2
        {0, 1, 1, 1, 1},                // 6 Tom 3
        {0, 1, 1, 1, 1},                // 7 Ride
        {0, 1, 1, 1, 1}                 // 8 Right Crash
    };

    //==============================================================================
    // Per-gem-type scales (applied uniformly to base + overlay in NoteRenderer)
    struct GemTypeScales
    {
        float normal  = 1.0f;   // Regular notes
        float cymbal  = 0.95f;  // Standalone cymbals (slightly smaller to avoid lane edge)
        float hopo    = 1.05f;  // Guitar HOPO
        float gTap    = 1.0f;   // Guitar tap
        float dGhost  = 1.0f;   // Drum note ghost
        float dAccent = 1.0f;   // Drum note accent
        float cGhost  = 1.0f;   // Drum cymbal ghost
        float cAccent = 1.0f;   // Drum cymbal accent
        float spGem   = 1.0f;   // Star power gem multiplier
        float spBar   = 1.0f;   // Star power bar multiplier (bars can't grow much)
    };

    //==============================================================================
    // Per-hit-type scales + flare config (applied in AnimationRenderer)
    struct HitTypeConfig
    {
        float ghost   = 0.7f;   // Drum ghost hit
        float accent  = 1.2f;   // Drum accent hit
        float hopo    = 0.9f;   // Guitar HOPO hit
        float tap     = 0.9f;   // Guitar tap hit
        float sp      = 1.0f;   // Star power hit
        bool  spWhiteFlare   = true;   // Use white flare for SP hits
        bool  tapPurpleFlare = true;   // Use purple flare for tap hits
    };

    //==============================================================================
    // Gem/bar base scales (ElementScale: width, height) — per-instrument
    constexpr ElementScale GUITAR_GEM_SCALE = {0.9f, 1.035f};   // 0.9 × {1.0, 1.15}
    constexpr ElementScale GUITAR_BAR_SCALE = {1.10f, 1.10f};
    constexpr ElementScale DRUM_GEM_SCALE   = {0.9f, 1.035f};   // matches guitar
    constexpr ElementScale DRUM_BAR_SCALE   = {1.05f, 1.05f};
    // Legacy single-value aliases (still referenced by SceneRenderer/DebugTuningPanel)
    constexpr ElementScale GEM_SCALE = GUITAR_GEM_SCALE;
    constexpr ElementScale BAR_SCALE = GUITAR_BAR_SCALE;

    //==============================================================================
    // Hit animation scales (HitScale: uniform scale, width, height)
    constexpr HitScale HIT_GEM_SCALE = {1.5f, 1.0f, 1.20f};
    constexpr HitScale HIT_BAR_SCALE = {1.0f, 1.0f, 1.0f};

    //==============================================================================
    // Perspective Parameters (per-instrument, compile-time constants)
    constexpr inline PerspectiveParams getGuitarPerspectiveParams()
    {
        return {
            100.0f,   // highwayDepth
            50.0f,    // playerDistance
            0.7f,     // perspectiveStrength
            0.65f,    // exponentialCurve
            1.0f,     // vanishingPointDepth (depth where progress=0; higher = gentler perspective)
            -0.23f,   // vanishingPointY (offset added to normY2; negative = VP higher on screen)
            1.07f,    // nearWidth (near-end width multiplier; <1 = narrower bottom, steeper angle)
            0.5f,     // xOffsetMultiplier
            16.0f,    // barNoteHeightRatio
            2.0f      // regularNoteHeightRatio
        };
    }

    constexpr inline PerspectiveParams getDrumPerspectiveParams()
    {
        return {
            100.0f,   // highwayDepth
            50.0f,    // playerDistance
            0.7f,     // perspectiveStrength
            0.65f,    // exponentialCurve
            1.0f,     // vanishingPointDepth
            -0.215f,  // vanishingPointY (drums: slightly less negative than guitar)
            1.07f,    // nearWidth
            0.5f,     // xOffsetMultiplier
            16.0f,    // barNoteHeightRatio
            2.0f      // regularNoteHeightRatio
        };
    }

    // Convenience: select by instrument
    constexpr inline PerspectiveParams getPerspectiveParams(bool isDrums)
    {
        return isDrums ? getDrumPerspectiveParams() : getGuitarPerspectiveParams();
    }

    // Legacy: returns guitar params (used by code that doesn't have isDrums context)
    constexpr inline PerspectiveParams getPerspectiveParams()
    {
        return getGuitarPerspectiveParams();
    }

}
