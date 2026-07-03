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
    constexpr Lane green  { 0xFF226D2C, 0xFF36B047 };
    constexpr Lane orange { 0xFFA85A14, 0xFFE88A20 };
    constexpr Lane purple { 0xFF5B2A8C, 0xFFAF3EE6 };   // bright = vivid open-bar purple
    constexpr Lane white  { 0xFFAEAEAE, 0xFFEAEAEA };

    inline juce::Colour dark   (const Lane& l) { return juce::Colour(l.dark); }
    inline juce::Colour bright (const Lane& l) { return juce::Colour(l.bright); }
}
