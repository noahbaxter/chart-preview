/*
    ==============================================================================

        GridlineRenderer.h
        Author:  Noah Baxter

        Gridline rendering extracted from SceneRenderer.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Utils/ChartTypes.h"
#include "../../Midi/Utils/TimeConverter.h"
#include "../Managers/AssetManager.h"
#include "../Utils/PositionConstants.h"
#include "../Utils/PositionMath.h"
#include "../Utils/DrawingConstants.h"
#include "../../UI/ControlConstants.h"
#include "../Painters/GridlinePainter.h"

class GridlineRenderer
{
public:
    GridlineRenderer(juce::ValueTree& state, AssetManager& assetManager);

    Part activePart = Part::GUITAR;
    bool writeMode = false;

    void populate(DrawCallMap& drawCallMap, const TimeBasedGridlineMap& gridlines,
                  double windowStartTime, double windowEndTime,
                  uint width, uint height,
                  float posEnd,
                  float gridlinePosOffset, float gridZOffset,
                  float farFadeEnd, float farFadeLen, float farFadeCurve);

private:
    juce::ValueTree& state;
    AssetManager& assetManager;

    // Cached per-populate call
    uint width = 0, height = 0;
    float posEnd = 0;

};
