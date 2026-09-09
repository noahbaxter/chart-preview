/*
    ==============================================================================

        HighwayRenderer.h
        Author:  Noah Baxter

        Shared base for every scrolling-highway sub-renderer (notes, sustains,
        animations, gridlines, text events). Owns the per-frame geometry state
        (active part, viewport size, highway end, far-fade, resolution scale,
        render config, lane coords/count) and the geometry helpers that used to
        be copy-pasted into each renderer: getColumnEdge, calculateOpacity,
        resolveLaneIndex. One setFrame() derives the render config + lane count
        from the active part so any highway (any lane count) is handled the same.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Utils/ChartTypes.h"
#include "../Utils/PositionConstants.h"
#include "../Utils/PositionMath.h"
#include "../Utils/DrawingConstants.h"
#include "../Utils/RenderTypeConfig.h"
#include "../../UI/ControlConstants.h"

class HighwayRenderer
{
public:
    using LaneCorners = PositionConstants::LaneCorners;
    using NormalizedCoordinates = PositionConstants::NormalizedCoordinates;

    // Active instrument for this frame. Set by SceneRenderer (directly or via
    // setFrame); drives config/lane-count derivation and every geometry helper.
    Part activePart = Part::GUITAR;

    // Active highway's lane coords (debug-tunable source, injected by SceneRenderer)
    // + lane count (derived from the render config). Generic: any number of lanes.
    const NormalizedCoordinates* laneCoords = nullptr;
    size_t laneCount = 0;
    // Z values in ColumnAdjust are tuned at REFERENCE_HEIGHT — callers multiply
    // by resScale at the read site.
    float resScale = 1.0f;

protected:
    // Shared per-frame state (was re-declared in every renderer).
    uint width = 0, height = 0;
    float posEnd = 0.0f;
    float farFadeEnd = 0.0f, farFadeLen = 0.0f, farFadeCurve = 0.0f;
    const PositionConstants::RenderTypeConfig* currentConfig = nullptr;

    // Set the shared geometry inputs for this frame. Derives the render config +
    // lane count from the part; laneCoords stays the injected tunable source.
    void setFrame(Part part, uint w, uint h, float posEnd_,
                  float ffEnd = 0.0f, float ffLen = 0.0f, float ffCurve = 0.0f)
    {
        activePart = part;
        width = w;
        height = h;
        posEnd = posEnd_;
        farFadeEnd = ffEnd;
        farFadeLen = ffLen;
        farFadeCurve = ffCurve;
        currentConfig = PositionConstants::getRenderTypeConfig(getRenderType(part));
        if (currentConfig) laneCount = currentConfig->laneCount;
    }

    // Project a lane column edge into screen space at the given position, routed
    // through the part's render type so any highway geometry is handled the same.
    LaneCorners getColumnEdge(float position, const NormalizedCoordinates& colCoords,
                              float sizeScale, float fretboardScale = 1.0f,
                              int bemaniLaneIdx = -1) const
    {
        return PositionMath::getColumnPosition(getRenderType(activePart), position, width, height,
                                               PositionConstants::HIGHWAY_POS_START, posEnd,
                                               colCoords, sizeScale, fretboardScale, bemaniLaneIdx);
    }

    float calculateOpacity(float position) const
    {
        if (PositionMath::bemaniMode) return 1.0f;
        return calculateFarFade(position, farFadeEnd, farFadeLen, farFadeCurve);
    }

    // Gem column -> index into laneCoords, part-generic and bounded to laneCount.
    // Guitar columns map straight through; drums remap the kick/2x virtual column.
    uint resolveLaneIndex(uint gemColumn) const
    {
        uint idx = isGuitarLike(activePart) ? gemColumn : drumColumnIndex(gemColumn, activePart);
        return (idx < laneCount) ? idx : 1u;
    }
};
