/*
    ==============================================================================

        GemArtCommon.h
        Author: Noah Baxter

        Shared primitives for procedural gem baking: W3C blend modes (the
        original Illustrator gem art is a greyscale base with overlay/color
        blended tint rects on top) and the per-pixel tint pass. Bake-time
        only; nothing here runs per frame.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>

namespace GemArt
{
    enum class Blend { Overlay, ColourBlend, Normal };

    // One tint rect over the baked greyscale base, matching the SVG layer
    // stacks (e.g. blue = one overlay layer; orange = color + overlay).
    struct TintLayer
    {
        juce::Colour colour;
        Blend mode = Blend::Overlay;
        float opacity = 1.0f;
        // Area the layer covers, as fractions of the content rect.
        juce::Rectangle<float> areaFrac { 0.0f, 0.0f, 1.0f, 1.0f };
    };

    // W3C compositing formulas, exposed for the gem_compare selftest.
    float blendOverlay(float backdrop, float source);
    juce::Colour blendColour(juce::Colour backdrop, juce::Colour source);

    // Applies layers in order over contentPx, only where img alpha > 0.
    // Blends at float precision and preserves the base alpha.
    void applyTintLayers(juce::Image& img, juce::Rectangle<float> contentPx,
                         const std::vector<TintLayer>& layers);
}
