/*
    ==============================================================================

        CymbalGemArt.h
        Author: Noah Baxter

        Procedural cymbal gems. The source PDFs use radial + mesh gradients,
        so the cymbal is modeled: concentric lifted ellipse rings (white edge
        lip, dark skirt, black grooves, light rim band) and a per-pixel cone
        with an angular highlight profile sampled from the original render.
        Everything is coloured from one lane colour (plus optional rim/lip
        accents), which is what makes purple and custom tints possible.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GemArtCommon.h"

namespace GemArt
{
    struct CymbalStyle
    {
        juce::Colour base;                       // lane colour (cone/skirt/rim derive from it)
        juce::Colour rim  { 0x00000000 };        // optional rim-band override (0 = derive)
        juce::Colour lip  { 0xffffffff };        // edge/lip colour
    };

    CymbalStyle cymbalStyle(juce::Colour lane);  // rim/lip derived
    CymbalStyle cymbalStyleWhite();              // grey base, gold rim + lip (OD)

    juce::Image bakeCymbal(const CymbalStyle& style,
                           juce::Rectangle<int> canvas,
                           juce::Rectangle<int> contentBounds);

    inline juce::Rectangle<int> cymContentBounds() { return { 0, 106, 1193, 385 }; }
}
