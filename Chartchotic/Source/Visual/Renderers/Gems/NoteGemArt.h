/*
    ==============================================================================

        NoteGemArt.h
        Author: Noah Baxter

        Procedural square note + hopo gems, interpreted from the original
        vector art (assets/archive/svg/tom *.svg, hopo *.svg). Both share one
        construction: chrome end caps, silver pillars, a greyscale body with
        radial shading, sheen + light streak, bottom bar, then blend-mode
        tint layers over the body. Geometry is expressed in the source SVG's
        viewBox units and scaled to the requested content bounds at bake time.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GemArtCommon.h"

namespace GemArt
{
    // All values in source-SVG viewBox units.
    struct TubeGemGeom
    {
        float vbW, vbH;
        float capOuterW, capOuterR;         // outer silver cap
        float insetX, insetTopY, insetR;    // black inset
        float chromeTopY, chromeR;          // inner chrome fill
        float pillarW;
        float artBottom;                    // y of the bottom bar's top edge
        float sheenBottom;                  // top sheen strip height
        float streakTop, streakBottom;      // light streak band
        float radCx, radCy, radR;           // body radial gradient
        float radTx, radTy, radSx, radSy;   //   gradientTransform translate/scale
    };

    TubeGemGeom noteGeom();     // tom blue.svg, viewBox 126.02 x 37.999
    TubeGemGeom hopoGeom();     // hopo blue.svg, viewBox 76.437 x 30.891

    // Original generated-PNG canvas and content placement (measured alpha
    // bboxes, verified by gem_compare). Baking with these keeps downstream
    // aspect/offset math identical to the PNG era.
    inline juce::Rectangle<int> noteCanvas()        { return { 0, 0, 1194, 598 }; }
    inline juce::Rectangle<int> noteContentBounds() { return { 72, 140, 1050, 317 }; }
    inline juce::Rectangle<int> hopoContentBounds() { return { 278, 170, 637, 257 }; }

    // Tint stacks matching the original per-colour SVG layers. capsGold is the
    // OD/star-power variant where caps+pillars are painted flat #ffb500.
    struct TubeGemStyle
    {
        std::vector<TintLayer> tints;
        bool capsGold = false;
    };

    TubeGemStyle tubeStyleOverlay(juce::Colour lane);   // blue / red / yellow
    TubeGemStyle tubeStyleGreen();
    TubeGemStyle tubeStyleOrange();
    TubeGemStyle tubeStyleWhite();                      // OD / star power

    juce::Image bakeTubeGem(const TubeGemGeom& geom,
                            juce::Rectangle<int> canvas,
                            juce::Rectangle<int> contentBounds,
                            const TubeGemStyle& style);
}
