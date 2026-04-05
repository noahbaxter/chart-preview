/*
    ==============================================================================

        GridlinePainter.h
        Author:  Noah Baxter

        Stateless gridline drawing. Extracted from GridlineRenderer.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Utils/ChartTypes.h"
#include "../Utils/PositionConstants.h"
#include "../Utils/PositionMath.h"
#include "../Utils/DrawingConstants.h"

namespace GridlinePainter
{
    struct Params
    {
        float position;
        juce::Image* markerImage;
        float opacity;
        Part activePart;
        uint viewportW, viewportH;
        float posEnd;
        float gridZOffset;
    };

    void paintPerspective(juce::Graphics& g, const Params& p);
    void paintBemani(juce::Graphics& g, const Params& p);
}
