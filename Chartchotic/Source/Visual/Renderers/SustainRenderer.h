/*
    ==============================================================================

        SustainRenderer.h
        Author:  Noah Baxter

        Sustain and lane rendering extracted from SceneRenderer.

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
#include "../Painters/LanePainter.h"

class SustainRenderer
{
public:
    SustainRenderer(juce::ValueTree& state, AssetManager& assetManager);

    Part activePart = Part::GUITAR;

    PositionConstants::LaneShapeConfig laneShape;

    void populate(DrawCallMap& drawCallMap, const TimeBasedSustainWindow& sustainWindow,
                  double windowStartTime, double windowEndTime,
                  uint width, uint height, bool showLanes, bool showSustains,
                  float posEnd,
                  float farFadeEnd, float farFadeLen, float farFadeCurve,
                  const PositionConstants::NormalizedCoordinates* laneCoordsGuitar,
                  const PositionConstants::NormalizedCoordinates* laneCoordsDrums);

private:
    juce::ValueTree& state;
    AssetManager& assetManager;

    // Cached per-populate call
    DrawCallMap* currentDrawCallMap = nullptr;
    uint width = 0, height = 0;
    float posEnd = 0;
    float farFadeEnd = 0, farFadeLen = 0, farFadeCurve = 0;
    const PositionConstants::NormalizedCoordinates* laneCoordsGuitar = nullptr;
    const PositionConstants::NormalizedCoordinates* laneCoordsDrums = nullptr;
    bool showLanes = true, showSustains = true;

    void drawSustain(const TimeBasedSustainEvent& sustain, double windowStartTime, double windowEndTime);
};
