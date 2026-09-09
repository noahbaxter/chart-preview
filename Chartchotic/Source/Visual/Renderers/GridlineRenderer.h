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
#include "../Geometry/PositionConstants.h"
#include "../Geometry/PositionMath.h"
#include "../Utils/DrawingConstants.h"
#include "../../UI/ControlConstants.h"
#include "HighwayRenderer.h"

class GridlineRenderer : public HighwayRenderer
{
public:
    GridlineRenderer(juce::ValueTree& state, AssetManager& assetManager);

    bool writeMode = false;   // When true, use write-mode opacities and paint STEP lines

    void populate(DrawCallMap& drawCallMap, const TimeBasedGridlineMap& gridlines,
                  double windowStartTime, double windowEndTime,
                  uint width, uint height,
                  float posEnd,
                  float gridlinePosOffset, float gridZOffset,
                  float farFadeEnd, float farFadeLen, float farFadeCurve);

private:
    juce::ValueTree& state;
    AssetManager& assetManager;
};
