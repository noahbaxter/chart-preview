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

//==============================================================================
// Opacity & Visual Effects
//==============================================================================

// Gem and note opacity
constexpr float OPACITY_FADE_START = 0.9f;   // Position where gem opacity starts fading
constexpr float SUSTAIN_OPACITY = 0.7f;      // Sustain note opacity
constexpr float LANE_OPACITY = 0.4f;         // Lane sustain opacity

// Gridline opacity by type
constexpr float MEASURE_OPACITY = 1.0f;      // Gridline opacity for measure lines
constexpr float BEAT_OPACITY = 0.4f;         // Gridline opacity for beat lines
constexpr float HALF_BEAT_OPACITY = 0.3f;    // Gridline opacity for half-beat lines

// Hit animation rendering opacity
constexpr float HIT_FLASH_OPACITY = 0.8f;    // Hit flash frame opacity at strikeline
constexpr float HIT_FLARE_OPACITY = 0.6f;    // Colored flare overlay opacity
