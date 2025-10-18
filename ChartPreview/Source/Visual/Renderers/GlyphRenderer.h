/*
    ==============================================================================

        GlyphRenderer.h
        Created by Claude Code (refactoring positioning logic)
        Author: Noah Baxter

        This file contains glyph (note) positioning and rendering logic.
        Uses PositionConstants for all positioning data.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Utils/PositionConstants.h"

// Windows compatibility
#if defined(_WIN32) || defined(_WIN64) || defined(__WINDOWS__) || defined(_MSC_VER)
    typedef unsigned int uint;
#endif

class GlyphRenderer
{
public:
    GlyphRenderer() = default;
    ~GlyphRenderer() = default;

    //==============================================================================
    // Guitar Glyph Positioning
    juce::Rectangle<float> getGuitarGlyphRect(uint gemColumn, float position, uint width, uint height);
    juce::Rectangle<float> getGuitarGridlineRect(float position, uint width, uint height);

    //==============================================================================
    // Drum Glyph Positioning
    juce::Rectangle<float> getDrumGlyphRect(uint gemColumn, float position, uint width, uint height);
    juce::Rectangle<float> getDrumGridlineRect(float position, uint width, uint height);

    //==============================================================================
    // Overlay Positioning
    juce::Rectangle<float> getOverlayGlyphRect(juce::Rectangle<float> glyphRect, bool isDrumAccent);

private:

    //==============================================================================
    // Core perspective calculation
    juce::Rectangle<float> createPerspectiveGlyphRect(
        float position,
        float normY1, float normY2,
        float normX1, float normX2,
        float normWidth1, float normWidth2,
        bool isBarNote,
        uint width, uint height
    );
};
