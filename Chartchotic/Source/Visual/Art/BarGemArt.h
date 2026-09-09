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

    // Elite Stomp/Splash pedal bar: a flat white plane with a raised pseudo-3D box, flat greys
    // (no gradients). Symmetric and parabolically arced so it sits along the highway's curved
    // gridlines. Self-contained (defines its own canvas). `arch` sets the arc depth in content px
    // (caller derives it from the shared curvature constant); `thickness` scales the vertical
    // extent. The content canvas is kStompBarWidth wide, so arch = |curvature| * kStompBarWidth *
    // spanFraction matches the gridline curvature over the bar's span.
    constexpr int kStompBarWidth = 2432;
    juce::Image bakeStompBar(float arch = 0.0f, float thickness = 1.0f);

    // Procedural gridline marker: a thin flat-grey parallelogram (diagonal-cut ends) bowed by
    // the SAME parabola the notes / stomp bar use. The caller derives `arch` from
    // NOTE_CURVATURE_DRUMS over the full board width, so gridlines and the stomp bar curve from
    // ONE source and pixel-match instead of the gridline baking a hand-guessed arc. `thickness`
    // is the bar height in content px; `colour` is the flat fill (grey + per-subdivision alpha);
    // `arch` bows the ends down (centre rises) by that many px. Canvas is kGridlineWidth wide.
    constexpr int kGridlineWidth = 2496;
    juce::Image bakeGridline(float thickness, juce::Colour colour, float arch);
}
