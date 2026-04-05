/*
    ==============================================================================

        TrackPainter.h
        Author:  Noah Baxter

        Stateless track/highway background drawing. Extracted from TrackRenderer.
        Handles dark fill, Bemani overlays, sidebar masks, and sidebar rails.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Utils/ChartTypes.h"
#include "../Utils/PositionConstants.h"
#include "../Utils/PositionMath.h"
#include "../Utils/DrawingConstants.h"

namespace TrackPainter
{
    /** Dark fretboard fill (perspective: baked image, Bemani: programmatic rect). */
    void paintBackground(juce::Graphics& g, int viewportW, int viewportH,
                         Part activePart, float posEnd,
                         const juce::Image& fadedTrackImage, bool showTrack);

    /** Paint from an externally-provided cached faded track image. */
    void paintFromCache(juce::Graphics& g, const juce::Image& cachedFadedTrack,
                        int viewportW, int viewportH,
                        Part activePart, float posEnd, bool showTrack);

    /** Bemani lane dividers and strikeline pads. */
    void paintBemaniOverlay(juce::Graphics& g, int viewportW, int viewportH,
                            Part activePart, float posEnd,
                            bool showLaneSeparators, bool showStrikeline);

    /** Opaque black masks outside the fretboard (clips sustain/note overflow in Bemani). */
    void paintBemaniSidebars(juce::Graphics& g, int viewportW, int viewportH,
                             Part activePart, float posEnd);

    /** Decorative sidebar rails in Bemani mode. */
    void paintBemaniRails(juce::Graphics& g, int viewportW, int viewportH,
                          Part activePart, float posEnd);

    /** Scrolling highway texture in Bemani mode. */
    void paintBemaniTexture(juce::Graphics& g, const juce::Image& sourceTexture,
                            float scrollOffset, float textureScale, float textureOpacity,
                            int targetW, int targetH,
                            Part activePart, float posEnd);
}
