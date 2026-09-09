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
#include "HighwayRenderer.h"

namespace PositionConstants { struct RenderTypeConfig; }

class SustainRenderer : public HighwayRenderer
{
public:
    SustainRenderer(juce::ValueTree& state, AssetManager& assetManager);

    PositionConstants::LaneShapeConfig laneShape;

    struct TintedSustain { int lane; double startTime; double endTime; juce::Colour colour; bool matchStart = false; };
    std::vector<TintedSustain> tintedSustains;

    void populate(DrawCallMap& drawCallMap, const TimeBasedSustainWindow& sustainWindow,
                  double windowStartTime, double windowEndTime,
                  uint width, uint height, bool showLanes, bool showSustains,
                  float posEnd,
                  float farFadeEnd, float farFadeLen, float farFadeCurve,
                  const PositionConstants::NormalizedCoordinates* laneCoords);

private:
    juce::ValueTree& state;
    AssetManager& assetManager;

    // Cached per-populate call (frame geometry state lives in HighwayRenderer)
    DrawCallMap* currentDrawCallMap = nullptr;
    bool showLanes = true, showSustains = true;

    void drawSustain(const TimeBasedSustainEvent& sustain, double windowStartTime, double windowEndTime);
    void drawSustainBody(juce::Graphics& g, uint gemColumn, float startPosition, float endPosition,
                         float opacity, float sustainWidth, juce::Colour colour, bool isLane);
};
