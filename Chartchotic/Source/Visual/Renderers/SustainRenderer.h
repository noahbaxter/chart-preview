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

namespace PositionConstants { struct RenderTypeConfig; }

class SustainRenderer
{
public:
    SustainRenderer(juce::ValueTree& state, AssetManager& assetManager);

    Part activePart = Part::GUITAR;

    PositionConstants::LaneShapeConfig laneShape;

    struct TintedSustain { int lane; double startTime; double endTime; juce::Colour colour; bool matchStart = false; };
    std::vector<TintedSustain> tintedSustains;

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
    const PositionConstants::RenderTypeConfig* currentConfig = nullptr;
    uint width = 0, height = 0;
    float posEnd = 0;
    float farFadeEnd = 0, farFadeLen = 0, farFadeCurve = 0;
    const PositionConstants::NormalizedCoordinates* laneCoordsGuitar = nullptr;
    const PositionConstants::NormalizedCoordinates* laneCoordsDrums = nullptr;
    bool showLanes = true, showSustains = true;

    using LaneCorners = PositionConstants::LaneCorners;
    using NormalizedCoordinates = PositionConstants::NormalizedCoordinates;

    LaneCorners getColumnEdge(float position, const NormalizedCoordinates& colCoords,
                              float sizeScale, float fretboardScale = 1.0f,
                              int bemaniLaneIdx = -1)
    {
        bool isDrums = isDrumLike(activePart);
        return PositionMath::getColumnPosition(isDrums, position, width, height,
                                               PositionConstants::HIGHWAY_POS_START, posEnd,
                                               colCoords, sizeScale, fretboardScale, bemaniLaneIdx);
    }

    float calculateOpacity(float position)
    {
        if (PositionMath::bemaniMode) return 1.0f;
        return calculateFarFade(position, farFadeEnd, farFadeLen, farFadeCurve);
    }

    void drawSustain(const TimeBasedSustainEvent& sustain, double windowStartTime, double windowEndTime);
    void drawSustainBody(juce::Graphics& g, uint gemColumn, float startPosition, float endPosition,
                         float opacity, float sustainWidth, juce::Colour colour, bool isLane);
};
