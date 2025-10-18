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
    struct NormalizedCoordinates
    {
        float normY1, normY2;      // Y position at far (1) and near (0) end
        float normX1, normX2;      // X position at far and near end
        float normWidth1, normWidth2;  // Width at far and near end
    };

    // 3D perspective calculation parameters
    struct PerspectiveParams
    {
        float highwayDepth;
        float playerDistance;
        float perspectiveStrength;
        float exponentialCurve;
        float xOffsetMultiplier;
        float barNoteHeightRatio;
        float regularNoteHeightRatio;
    };

    //==============================================================================
    // Size & Scale Factors
    constexpr float GEM_SIZE = 0.9f;                    // Regular gem/note scaling factor
    constexpr float BAR_SIZE = 0.95f;                   // Bar note (kick/open) scaling factor
    constexpr float GRIDLINE_SIZE = 0.9f;               // Gridline scaling factor
    constexpr float SUSTAIN_WIDTH = 0.15f;              // Sustain width multiplier
    constexpr float SUSTAIN_OPEN_WIDTH = 0.8f;          // Open sustain width multiplier
    constexpr float LANE_WIDTH = 1.1f;                  // Lane width multiplier
    constexpr float LANE_OPEN_WIDTH = 0.9f;             // Open lane width multiplier

    //==============================================================================
    // Animation Scaling Factors (for rendering enlarged animations at strikeline)
    constexpr float KICK_ANIMATION_WIDTH_SCALE = 1.3f;
    constexpr float KICK_ANIMATION_HEIGHT_SCALE = 4.2f;
    constexpr float HIT_ANIMATION_WIDTH_SCALE = 1.6f;
    constexpr float HIT_ANIMATION_HEIGHT_SCALE = 2.8f;

    //==============================================================================
    // Sustain Geometry Constants
    constexpr float SUSTAIN_WIDTH_MULTIPLIER = 0.8f;    // Sustain slightly narrower than gems
    constexpr float SUSTAIN_CAP_RADIUS_SCALE = 0.25f;   // Scale for rounded sustain caps
    constexpr float SUSTAIN_MARGIN_SCALE = 0.1f;        // Small margin above/below center

    //==============================================================================
    // Special Visual Scales
    constexpr float DRUM_ACCENT_OVERLAY_SCALE = 1.1232876712f;  // Drum accent overlay base scale

    //==============================================================================
    // Coordinate lookup tables (column 0 is always open/kick, columns 1-5 are pads)
    constexpr NormalizedCoordinates guitarGlyphCoords[] = {
        {0.745f, 0.234f, 0.16f, 0.34f, 0.68f, 0.32f},   // Open note
        {0.73f, 0.22f, 0.20f, 0.363f, 0.125f, 0.065f},  // Col 1 - Green
        {0.73f, 0.22f, 0.320f, 0.412f, 0.125f, 0.065f}, // Col 2 - Red
        {0.73f, 0.22f, 0.440f, 0.465f, 0.125f, 0.065f}, // Col 3 - Yellow
        {0.73f, 0.22f, 0.557f, 0.524f, 0.125f, 0.065f}, // Col 4 - Blue
        {0.73f, 0.22f, 0.673f, 0.580f, 0.125f, 0.065f}  // Col 5 - Orange
    };

    constexpr NormalizedCoordinates guitarLaneCoords[] = {
        {0.73f, 0.234f, 0.16f, 0.34f, 0.68f, 0.32f},    // Open note
        {0.71f, 0.22f, 0.227f, 0.363f, 0.105f, 0.055f}, // Col 1 - Green
        {0.71f, 0.22f, 0.322f, 0.412f, 0.125f, 0.065f}, // Col 2 - Red
        {0.71f, 0.22f, 0.440f, 0.465f, 0.125f, 0.065f}, // Col 3 - Yellow
        {0.71f, 0.22f, 0.555f, 0.524f, 0.125f, 0.065f}, // Col 4 - Blue
        {0.71f, 0.22f, 0.670f, 0.580f, 0.125f, 0.065f}  // Col 5 - Orange
    };

    constexpr NormalizedCoordinates drumGlyphCoords[] = {
        {0.75f, 0.239f, 0.16f, 0.34f, 0.68f, 0.32f},     // Kick
        {0.72f, 0.22f, 0.22f, 0.365f, 0.147f, 0.0714f},  // Col 1 - Red
        {0.72f, 0.22f, 0.360f, 0.430f, 0.147f, 0.0714f}, // Col 2 - Yellow
        {0.72f, 0.22f, 0.497f, 0.495f, 0.147f, 0.0714f}, // Col 3 - Blue
        {0.72f, 0.22f, 0.640f, 0.564f, 0.147f, 0.0714f}  // Col 4 - Green
    };

    constexpr NormalizedCoordinates drumLaneCoords[] = {
        {0.735f, 0.239f, 0.16f, 0.34f, 0.68f, 0.32f},    // Kick
        {0.70f, 0.22f, 0.222f, 0.37f, 0.147f, 0.0714f},  // Col 1 - Red
        {0.70f, 0.22f, 0.360f, 0.430f, 0.147f, 0.0714f}, // Col 2 - Yellow
        {0.70f, 0.22f, 0.497f, 0.495f, 0.147f, 0.0714f}, // Col 3 - Blue
        {0.70f, 0.22f, 0.630f, 0.564f, 0.147f, 0.0714f}  // Col 4 - Green
    };

    //==============================================================================
    // Perspective Parameters (compile-time constant)
    constexpr inline PerspectiveParams getPerspectiveParams()
    {
        return {
            100.0f,   // highwayDepth
            50.0f,    // playerDistance
            0.7f,     // perspectiveStrength
            0.5f,     // exponentialCurve
            0.5f,     // xOffsetMultiplier
            16.0f,    // barNoteHeightRatio
            2.0f      // regularNoteHeightRatio
        };
    }

    //==============================================================================
    // Guitar Positioning Data
    constexpr inline NormalizedCoordinates getGuitarOpenNoteCoords()
    {
        return guitarGlyphCoords[0];
    }

    constexpr inline NormalizedCoordinates getGuitarNoteCoords(uint gemColumn)
    {
        constexpr size_t GUITAR_GLYPH_SIZE = 6;
        uint index = (gemColumn < GUITAR_GLYPH_SIZE) ? gemColumn : 1;
        return guitarGlyphCoords[index];
    }

    constexpr inline NormalizedCoordinates getDrumKickCoords()
    {
        return drumGlyphCoords[0];
    }

    constexpr inline NormalizedCoordinates getDrumPadCoords(uint gemColumn)
    {
        constexpr size_t DRUM_GLYPH_SIZE = 5;
        uint index = (gemColumn < DRUM_GLYPH_SIZE) ? gemColumn : 1;
        return drumGlyphCoords[index];
    }
}
