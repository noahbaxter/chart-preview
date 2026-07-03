/*
    ==============================================================================

        LaneColours.h
        Author: Noah Baxter

        Single source of truth for the gameplay lane colours (the note/pad hues:
        red, yellow, blue, green, orange, purple, white). Every consumer that
        needs a lane colour, the strikeline pads (TrackRenderer) and the gem
        tints (AssetManager), reads from here, so a colour changes in ONE place.

        Each lane is a { dark, bright } pair (ARGB): the pad gradient runs dark
        (top) -> bright (bottom), and gem tint ramps use the same pair.

        Caveat: colours here fully drive the strikeline and any tint-derived gem
        (currently the purple L-Crash cymbal, rebuilt from greyscale in
        AssetManager). The blue/red/yellow/green gem PNGs still have their colour
        baked into the art, so they only follow these constants once the gems
        move to the greyscale-master + tint path (see BACKLOG). Keep the values
        here matched to the PNG art until then.

    ==============================================================================
*/

#pragma once

#include <juce_graphics/juce_graphics.h>

namespace LaneColours
{
    struct Lane { juce::uint32 dark, bright; };

    constexpr Lane red    { 0xFFA91E1A, 0xFFEB1C22 };
    constexpr Lane yellow { 0xFF987F0B, 0xFFFFD800 };
    constexpr Lane blue   { 0xFF0D447F, 0xFF1678E4 };
    constexpr Lane green  { 0xFF145A20, 0xFF1FA838 };   // deepened to match baked art (was 226D2C/36B047)
    constexpr Lane orange { 0xFF8A4A08, 0xFFE97500 };   // purer, less amber (was A85A14/E88A20)
    constexpr Lane purple { 0xFF5B2A8C, 0xFFAF3EE6 };   // bright = vivid open-bar purple
    constexpr Lane white  { 0xFFAEAEAE, 0xFFEAEAEA };

    // Kick / bar tube colours (tint the greyscale bar master). Open bars reuse `purple`.
    constexpr Lane kick   { 0xFF8A3D06, 0xFFF16E0B };   // kick bar: red amber (baked art)
    constexpr Lane kick2x { 0xFF8A2806, 0xFFF14A0B };   // 2x-kick bar: distinct redder cue

    inline juce::Colour dark   (const Lane& l) { return juce::Colour(l.dark); }
    inline juce::Colour bright (const Lane& l) { return juce::Colour(l.bright); }
}

// Structural / neutral render colours (highway backing, strikeline pad bevels, side
// rails). Kept here so every colour in the renderer lives in ONE place — no inline
// juce::Colour literals scattered through the drawing code. The juce::Colour is built
// once, here; consumers just name the constant.
namespace TrackColours
{
    inline const juce::Colour highwayFill     = juce::Colour(0xFF111111);  // highway backing fill
    inline const juce::Colour sheenWhite      = juce::Colour(0x40FFFFFF);  // translucent white sheen
    inline const juce::Colour debugPoly       = juce::Colour(0xFFFF0000);  // debug poly-shade overlay

    // Strikeline pad bevel (TrackRenderer FretColour gradient stops)
    inline const juce::Colour padBevelInner   = juce::Colour(0xFF8C8C8C);
    inline const juce::Colour padBevelOuter   = juce::Colour(0xFF606060);
    inline const juce::Colour padBevelDark    = juce::Colour(0xFF1A1A1A);
    inline const juce::Colour padFallbackDark = juce::Colour(0xFF888888);  // unmapped lane
    inline const juce::Colour padFallbackLite = juce::Colour(0xFFBBBBBB);

    // Side rail bands (ProceduralTrackArt)
    inline const juce::Colour railInner       = juce::Colour(0xFF404040);
    inline const juce::Colour railLine        = juce::Colour(0xFF606060);
    inline const juce::Colour railBlack       = juce::Colour(0xFF060606);

    // Bar (kick/open tube) casing — the neutral dark rim the glowing tube sits in.
    // Colour-independent, so the bar tint ramps grey edges -> lane colour -> hot core.
    inline const juce::Colour barCasingDark   = juce::Colour(0xFF1F1F1F);
    inline const juce::Colour barCasingGrey   = juce::Colour(0xFF676565);
}
