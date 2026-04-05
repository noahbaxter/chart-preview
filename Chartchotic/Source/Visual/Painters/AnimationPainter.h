/*
    ==============================================================================

        AnimationPainter.h
        Author:  Noah Baxter

        Stateless hit animation drawing. Extracted from AnimationRenderer.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Utils/ChartTypes.h"
#include "../Managers/AnimationManager.h"
#include "../Utils/PositionConstants.h"
#include "../Utils/PositionMath.h"
#include "../Utils/DrawingConstants.h"

namespace AnimationPainter
{
    struct KickParams
    {
        AnimationConstants::HitAnimation anim;
        juce::Image* animFrame;
        juce::Image* flareImage;   // null if no SP flare
        float noteAspect;          // aspect ratio for Bemani bar rect sizing
        Part activePart;
        uint viewportW, viewportH;
        float posEnd, strikePos;
        PositionConstants::NormalizedCoordinates laneCoords;
        float hitBarZOffset;
        PositionConstants::HitScale hitBarScale;
        PositionConstants::CoordinateOffset offset;
        float userBarScale;        // pre-resolved from state
    };

    struct FretParams
    {
        AnimationConstants::HitAnimation anim;
        juce::Image* hitFrame;
        juce::Image* flareImage;
        Part activePart;
        uint viewportW, viewportH;
        float posEnd, strikePos;
        PositionConstants::NormalizedCoordinates laneCoords;
        float sizeScale;
        int bemaniLaneIdx;
        float zOffset;
        float noteCurvature;
        float dynScale;
        PositionConstants::HitScale hitScale;
        PositionConstants::CoordinateOffset offset;
    };

    void paintKick(juce::Graphics& g, const KickParams& p);
    void paintFret(juce::Graphics& g, const FretParams& p);
}
