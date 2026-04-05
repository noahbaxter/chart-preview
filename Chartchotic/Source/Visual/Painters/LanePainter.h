/*
    ==============================================================================

        LanePainter.h
        Author:  Noah Baxter

        Stateless sustain/lane polygon drawing. Extracted from SustainRenderer.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Utils/ChartTypes.h"
#include "../Utils/PositionConstants.h"
#include "../Utils/PositionMath.h"
#include "../Utils/DrawingConstants.h"

namespace LanePainter
{
    struct Params
    {
        uint gemColumn;
        Part activePart;
        float startPosition, endPosition;
        float opacity, sustainWidth;
        juce::Colour colour;
        bool isLane;
        // Geometry context
        uint viewportW, viewportH;
        float posEnd;
        // Lane coords for this column (copied — lambdas outlive the caller)
        PositionConstants::NormalizedCoordinates laneCoords;
        float laneScale;
        int bemaniLaneIdx;      // -1 for perspective mode
        // Lane shape (only used when isLane=true)
        PositionConstants::LaneShapeConfig laneShape;
        // Fade (for gradient fill near far end)
        float farFadeEnd, farFadeLen, farFadeCurve;
    };

    void paint(juce::Graphics& g, const Params& p);
}
