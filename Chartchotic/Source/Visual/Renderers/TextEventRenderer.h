/*
    ==============================================================================

        TextEventRenderer.h
        Author:  Noah Baxter

        Renders text event markers (e.g. disco flip start/end) as translucent
        "force field" bands on the highway.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Utils/ChartTypes.h"
#include "../Utils/PositionConstants.h"
#include "../Utils/PositionMath.h"
#include "../Utils/DrawingConstants.h"

class TextEventRenderer
{
public:
    Part activePart = Part::GUITAR;

    void populate(DrawCallMap& drawCallMap,
                  const std::vector<TimeBasedFlipRegion>& flipRegions,
                  double windowStartTime, double windowEndTime,
                  uint width, uint height,
                  float posEnd,
                  float farFadeEnd, float farFadeLen, float farFadeCurve);

    void populateEventMarkers(DrawCallMap& drawCallMap,
                              const TimeBasedEventMarkers& markers,
                              double windowStartTime, double windowEndTime,
                              uint width, uint height,
                              float posEnd,
                              float farFadeEnd, float farFadeLen, float farFadeCurve);

private:
    uint width = 0, height = 0;
    float posEnd = 0;

    using LaneCorners = PositionConstants::LaneCorners;
    using NormalizedCoordinates = PositionConstants::NormalizedCoordinates;

    LaneCorners getColumnEdge(float position, const NormalizedCoordinates& colCoords,
                              float sizeScale, float fretboardScale = 1.0f)
    {
        bool isDrums = isDrumLike(activePart);
        return PositionMath::getColumnPosition(getRenderType(activePart), position, width, height,
                                               PositionConstants::HIGHWAY_POS_START, posEnd,
                                               colCoords, sizeScale, fretboardScale);
    }

    void drawMarker(juce::Graphics& g, float position, const juce::String& label,
                    float fadeOpacity);
};
