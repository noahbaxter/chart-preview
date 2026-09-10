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
#include "../Utils/LaneColours.h"

namespace GemArt
{
    // Vertical colour ramp through the tube, top (0) to bottom (1).
    using BarRamp = std::vector<std::pair<float, juce::Colour>>;

    BarRamp barRampWhite();     // star power / SP bar (near-greyscale)
    BarRamp barRampKick();      // orange
    BarRamp barRampKick2x();    // red-orange
    BarRamp barRampOpen();      // purple

    // `thickness` squeezes the tube and its caps about the bar's centreline. The arc, the
    // canvas and the centreline are untouched, so a thin bar curves exactly like a fat one
    // and the gems that sit on it don't move. Elite bakes thin (kEliteBarThickness) because
    // its 9 lanes shrink the gems while a full-width bar keeps 4-lane proportions.
    constexpr float kEliteBarThickness = 0.5f;

    // Elite kick dynamics, as a placeholder treatment until there is real art. An accent
    // bakes at double thickness with a bright line down its centreline; a ghost reuses the
    // normal bake and is drawn at kBarGhostOpacity. Both keep the arc and the centreline, so
    // a dynamic kick sits exactly where a normal one would.
    constexpr float kAccentBarThickness = kEliteBarThickness * 2.0f;
    constexpr float kBarGhostOpacity    = 0.45f;

    // `centreLineAlpha` > 0 draws that bright line, as a fraction of the tube height.
    constexpr float kAccentLineHeight = 0.16f;

    juce::Image bakeBar(const BarRamp& ramp,
                        juce::Rectangle<int> canvas,
                        juce::Rectangle<int> contentBounds,
                        float thickness = 1.0f,
                        float centreLineAlpha = 0.0f);

    inline juce::Rectangle<int> barCanvas()        { return { 0, 0, 2432, 152 }; }
    inline juce::Rectangle<int> barContentBounds() { return { 86, 0, 2260, 152 }; }

    // Elite Stomp/Splash pedal bar: a flat plane with a raised pseudo-3D box, flat shades (no
    // gradients). Symmetric and parabolically arced so it sits along the highway's curved
    // gridlines. Self-contained (defines its own canvas). `arch` sets the arc depth in content px
    // (caller derives it from the shared curvature constant); `thickness` scales the vertical
    // extent. The content canvas is kStompBarWidth wide, so arch = |curvature| * kStompBarWidth *
    // spanFraction matches the gridline curvature over the bar's span.
    // `face` tints the whole bar: it IS the top-face shade, and the front / end-cap shades are
    // scaled off it. `edgeWidth` > 0 adds a keyline of `edge` around the outer silhouette,
    // leaving the body filled.
    constexpr int kStompBarWidth = 2432;
    juce::Image bakeStompBar(float arch = 0.0f, float thickness = 1.0f,
                             juce::Colour face = juce::Colour(0xffffffff),
                             juce::Colour edge = juce::Colour(0x00000000),
                             float edgeWidth = 0.0f);

    // The hi-hat lane's own yellow. Stomp (the foot click) fills with it flat; Splash keeps the
    // white body and takes it as a gold keyline.
    inline const juce::Colour kStompBarFace = LaneColours::bright(LaneColours::yellow);
    inline const juce::Colour kStompBarBody = juce::Colour(0xffffffff);
    constexpr float kStompBarEdgeWidth = 20.0f;

    // Procedural gridline marker: a thin flat-grey parallelogram (diagonal-cut ends) bowed by
    // the SAME parabola the notes / stomp bar use. The caller derives `arch` from
    // NOTE_CURVATURE_DRUMS over the full board width, so gridlines and the stomp bar curve from
    // ONE source and pixel-match instead of the gridline baking a hand-guessed arc. `thickness`
    // is the bar height in content px; `colour` is the flat fill (grey + per-subdivision alpha);
    // `arch` bows the ends down (centre rises) by that many px. Canvas is kGridlineWidth wide.
    constexpr int kGridlineWidth = 2496;
    juce::Image bakeGridline(float thickness, juce::Colour colour, float arch);
}
