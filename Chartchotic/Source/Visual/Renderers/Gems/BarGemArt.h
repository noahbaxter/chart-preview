/*
    ==============================================================================

        BarGemArt.h
        Author: Noah Baxter

        Procedural bar notes (kick / 2x kick / open / star power). The source
        PDFs use mesh gradients nothing can draw natively, so the tube is
        modeled instead: a constant-height glossy tube whose vertical colour
        ramp was sampled from the original PNGs, bent along a parabolic arc,
        with tilted dark end caps. Verified against assets/generated/bars/.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GemArtCommon.h"

namespace GemArt
{
    // Vertical colour ramp through the tube, top (0) to bottom (1).
    using BarRamp = std::vector<std::pair<float, juce::Colour>>;

    BarRamp barRampWhite();     // star power / SP bar (near-greyscale)
    BarRamp barRampKick();      // orange
    BarRamp barRampKick2x();    // red-orange
    BarRamp barRampOpen();      // purple

    juce::Image bakeBar(const BarRamp& ramp,
                        juce::Rectangle<int> canvas,
                        juce::Rectangle<int> contentBounds);

    inline juce::Rectangle<int> barCanvas()        { return { 0, 0, 2432, 152 }; }
    inline juce::Rectangle<int> barContentBounds() { return { 86, 0, 2260, 152 }; }
}
