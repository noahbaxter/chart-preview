/*
    ==============================================================================

        NotePainter.h
        Author:  Noah Baxter

        Stateless note overlay geometry. Extracted from HighwayComponent.
        Computes screen-space rects and curved paths for notes.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Utils/ChartTypes.h"
#include "../Utils/PositionConstants.h"
#include "../Utils/PositionMath.h"
#include "../Utils/DrawingConstants.h"

namespace NotePainter
{
    // =========================================================================
    // Ghost cursor / selection overlay (screen-space, used by HighwayComponent)
    // =========================================================================

    struct NoteRect
    {
        float screenLeftX, screenRightX, screenCenterY, screenH;
        float renderLeftX, renderRightX;
        float curvature, arcOffset, sy;
        float position;
        bool isBar;
    };

    NoteRect computeRect(float position, int lane, Part activePart,
                         int renderW, int renderH, float posEnd, int topOverflow,
                         bool stretchToFill, int componentW, int componentH,
                         float foreshorten, float gemZOffset, float barZOffset,
                         float noteCurvature);

    juce::Path buildCurvedPath(const NoteRect& rect, Part activePart,
                               int renderW, int renderH,
                               float expand = 0.0f);

    // =========================================================================
    // Gem rect computation (render-space, used by NoteRenderer + drag-to-place)
    // =========================================================================

    struct GemParams
    {
        float position;
        int   gemColumn;
        bool  isBar, isDrums;
        uint  viewportW, viewportH;
        float posEnd;
        float imageAspect;
        float sizeScale;          // BAR_SIZE or GEM_SIZE
        float userScale;          // from state
        float wScale, hScale;     // composite: base * type * SP * column (used for gem + overlay)
        float foreshorten;
        float rawZOffset;         // before perspective scaling
        float strikeWidth;        // for perspective Z scaling (1.0 in bemani)
        float curvature;
        PositionConstants::NormalizedCoordinates laneCoords;
        int   bemaniLaneIdx;
        float bemaniNudgeY;       // 0 if not bemani
        bool  hasOverlay;
        PositionConstants::OverlayAdjust overlayAdj;
    };

    struct GemRects
    {
        juce::Rectangle<float> glyphRect;         // base rect (for curved cache sizing)
        juce::Rectangle<float> drawRect;          // final rect (straight path)
        float arcOffset;
        float perspZOffset;                        // perspective-scaled Z (for curved path)
        juce::Rectangle<float> overlayGlyphRect;  // overlay base (when hasOverlay)
        juce::Rectangle<float> overlayDrawRect;   // overlay final (when hasOverlay)
    };

    /** Compute render-space rects for a gem given pre-resolved orchestration params. */
    GemRects computeGemRects(const GemParams& p);

    /** Compute the draw rect for a curved (pre-warped) image from cache entry data. */
    juce::Rectangle<float> computeCurvedDrawRect(
        const juce::Rectangle<float>& glyphRect,
        float curvedImageAspect, float contentYOff,
        float arcOffset, float wScale, float hScale, float zOff);

    // =========================================================================
    // Shared helpers
    // =========================================================================

    /** Scale a rect around its center with w/h factors and Y offset. */
    juce::Rectangle<float> scaleRect(juce::Rectangle<float> r,
                                     float wScale, float hScale, float yOff);

    /** Normalized distance of a column from fretboard center (-1..1). */
    float getColumnDistFromCenter(const PositionConstants::NormalizedCoordinates& colCoords,
                                  bool isDrums);

    /** Center-scale overlay rect relative to base glyph rect. */
    juce::Rectangle<float> getOverlayGlyphRect(juce::Rectangle<float> glyphRect,
                                                const PositionConstants::OverlayAdjust& adj);

    /** Draw a gem glyph image at a rect. */
    void paintGem(juce::Graphics& g, const juce::Image& glyphImage,
                  juce::Rectangle<float> destRect, float opacity);
}
