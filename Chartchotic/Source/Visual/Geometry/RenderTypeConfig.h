/*
    ==============================================================================

        RenderTypeConfig.h
        Author: Noah Baxter

        Per-render-style data bundle. Replaces paired guitar / drum lookup
        tables and isDrums?X:Y branches in renderers with a single const-
        pointer indirection: `config->X`.

    ==============================================================================
*/

#pragma once

#include "PositionConstants.h"
#include "../../UI/ControlConstants.h"

namespace PositionConstants
{
    // POD bundle of per-render-style data. Static-lifetime singletons
    // returned by getRenderTypeConfig(). Holds data identity (which table,
    // which accessor) only — genuine behavior branches stay in renderer
    // code and use isDrumLike() / isGuitarLike().
    struct RenderTypeConfig
    {
        size_t laneCount;

        const NormalizedCoordinates* fretboardCoords;    // single value
        const NormalizedCoordinates* bezierLaneCoords;   // base of laneCount-sized array
        const InstrumentOffsets*     instOffsets;
        const ColumnAdjust*          colAdjust;          // base of laneCount-sized array
        const CoordinateOffset*      animOffsets;        // base of laneCount-sized array

        // Returns by value so DEBUG (reads PositionMath::debugPerspParams*
        // for live tuning) and RELEASE (constexpr defaults) share one path.
        PerspectiveParams (*getPerspectiveParams)();

        // Bemani accessors read live fields on the global bemaniConfig;
        // function pointers (not pre-resolved floats) preserve live tuning.
        float (*bemaniGemNudge)();
        float (*bemaniLaneEndPx)();
        float (*bemaniBarLaneEndPx)();
        float (*bemaniHwyScale)();
        float (*bemaniHitBarNudge)();

        // How much wider this render style's highway CANVAS is vs a normal one. 1.0 for
        // guitar / 4-lane drums; elite is wider so its 8 lanes get more room. Used by the
        // highway layout (wider slot / aspect) and the render harness to size the render
        // width -- the board itself stays a fixed fraction of that width, so nothing clips.
        float boardWidthScale = 1.0f;
    };

    // Returns nullptr for unsupported RenderTypes. Currently supplies
    // FIVE_FRET (guitar) and FOUR_LANE_DRUMS — the only highway-renderable
    // render types. Callers are gated upstream by isHighwayRenderable(),
    // so nullptr should never reach a renderer in practice. A deref crash
    // is intentional, to flag the gap loudly rather than render the wrong
    // style silently.
    const RenderTypeConfig* getRenderTypeConfig(RenderType type);
}
