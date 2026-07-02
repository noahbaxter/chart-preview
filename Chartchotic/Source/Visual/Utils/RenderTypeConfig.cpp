/*
    ==============================================================================

        RenderTypeConfig.cpp
        Author: Noah Baxter

    ==============================================================================
*/

#include "RenderTypeConfig.h"
#include "PositionMath.h"
#include "BemaniConfig.h"

namespace PositionConstants
{
namespace
{
    PerspectiveParams getGuitarPP()
    {
    #ifdef DEBUG
        return PositionMath::debugPerspParamsGuitar;
    #else
        return getGuitarPerspectiveParams();
    #endif
    }

    PerspectiveParams getDrumPP()
    {
    #ifdef DEBUG
        return PositionMath::debugPerspParamsDrums;
    #else
        return getDrumPerspectiveParams();
    #endif
    }

    float bemaniGemNudgeGuitar()     { return bemaniConfig.gemNudgeGuitar; }
    float bemaniGemNudgeDrums()      { return bemaniConfig.gemNudgeDrums; }
    float bemaniLaneEndPxGuitar()    { return bemaniConfig.laneEndPxGuitar; }
    float bemaniLaneEndPxDrums()     { return bemaniConfig.laneEndPxDrums; }
    float bemaniBarLaneEndPxGuitar() { return bemaniConfig.barLaneEndPxGuitar; }
    float bemaniBarLaneEndPxDrums()  { return bemaniConfig.barLaneEndPxDrums; }
    float bemaniHwyScaleGuitar()     { return bemaniConfig.hwyScaleGuitar; }
    float bemaniHwyScaleDrums()      { return bemaniConfig.hwyScaleDrums; }
    float bemaniHitBarNudgeGuitar()  { return bemaniConfig.hitBarNudgeGuitar; }
    float bemaniHitBarNudgeDrums()   { return bemaniConfig.hitBarNudgeDrums; }

    const RenderTypeConfig guitarFiveFretConfig = {
        GUITAR_LANE_COUNT,
        &guitarFretboardCoords,
        guitarBezierLaneCoords,
        &GUITAR_OFFSETS,
        GUITAR_COL_ADJUST,
        GUITAR_ANIMATION_OFFSETS,
        &getGuitarPP,
        &bemaniGemNudgeGuitar,
        &bemaniLaneEndPxGuitar,
        &bemaniBarLaneEndPxGuitar,
        &bemaniHwyScaleGuitar,
        &bemaniHitBarNudgeGuitar,
    };

    const RenderTypeConfig drumsFourLaneConfig = {
        DRUM_LANE_COUNT,
        &drumFretboardCoords,
        drumBezierLaneCoords,
        &DRUM_OFFSETS,
        DRUM_COL_ADJUST,
        DRUM_ANIMATION_OFFSETS,
        &getDrumPP,
        &bemaniGemNudgeDrums,
        &bemaniLaneEndPxDrums,
        &bemaniBarLaneEndPxDrums,
        &bemaniHwyScaleDrums,
        &bemaniHitBarNudgeDrums,
    };

    const RenderTypeConfig eliteDrumsConfig = {
        ELITE_DRUM_LANE_COUNT,
        &eliteDrumFretboardCoords,
        eliteDrumBezierLaneCoords.data(),
        &DRUM_OFFSETS,
        ELITE_DRUM_COL_ADJUST,
        ELITE_DRUM_ANIMATION_OFFSETS,
        &getDrumPP,
        &bemaniGemNudgeDrums,
        &bemaniLaneEndPxDrums,
        &bemaniBarLaneEndPxDrums,
        &bemaniHwyScaleDrums,
        &bemaniHitBarNudgeDrums,
    };
} // anonymous namespace

const RenderTypeConfig* getRenderTypeConfig(RenderType type)
{
    switch (type)
    {
        case RenderType::FIVE_FRET:       return &guitarFiveFretConfig;
        case RenderType::FOUR_LANE_DRUMS: return &drumsFourLaneConfig;
        case RenderType::ELITE_DRUMS:     return &eliteDrumsConfig;
        default:                          return nullptr;
    }
}

} // namespace PositionConstants
