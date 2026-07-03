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
#include "HighwayRenderer.h"

class TextEventRenderer : public HighwayRenderer
{
public:
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
    void drawMarker(juce::Graphics& g, float position, const juce::String& label,
                    float fadeOpacity);
};
