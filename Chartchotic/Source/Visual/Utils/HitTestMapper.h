/*
  ==============================================================================

    HitTestMapper.h
    Author: Noah Baxter

    Reverse-maps screen coordinates to musical positions (time + lane).
    Inverts the forward rendering math in PositionMath to enable mouse
    interaction on the highway.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PositionMath.h"

struct HitTestResult
{
    float normalizedPosition = 0.0f;  // 0 = strikeline, positive = toward far end
    double timeFromCursor = 0.0;      // seconds offset from cursor (add to windowStartTime)
    int laneIndex = -1;               // 0-based lane (guitar: 0=open,1-5=frets; drums: 0=kick,1-4=pads), -1 = outside
    bool valid = false;               // false if click is outside the highway
};

class HitTestMapper
{
public:
    HitTestMapper() = default;

    /** Map a screen pixel to a normalized highway position and lane.
     *
     *  @param screenX, screenY    Pixel coordinates relative to the highway component
     *  @param viewportWidth, viewportHeight  Highway component dimensions
     *  @param windowStartTime     Start of visible time window (seconds from cursor)
     *  @param windowEndTime       End of visible time window
     *  @param isDrums             True for drum highways (4 lanes), false for guitar (5+open)
     *  @param farFadeEnd          Highway length (farFadeEnd from SceneRenderer)
     *  @param fretboardScale      Fretboard width scale (FRETBOARD_SCALE or debug override)
     */
    HitTestResult hitTest(float screenX, float screenY,
                          uint viewportWidth, uint viewportHeight,
                          double windowStartTime, double windowEndTime,
                          bool isDrums, float farFadeEnd,
                          float fretboardScale = PositionConstants::FRETBOARD_SCALE) const;

private:
    /** Invert the Y coordinate to get normalized position (0=strike, positive=far end).
     *  Returns < 0 if off-highway. */
    float invertYToPosition(float screenY, uint viewportWidth, uint viewportHeight,
                            bool isDrums, float farFadeEnd) const;

    /** Given a position (depth on highway), find which lane the X coordinate falls in. */
    int identifyLane(float screenX, float position,
                     uint viewportWidth, uint viewportHeight,
                     bool isDrums, float fretboardScale) const;
};
