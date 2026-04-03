/*
  ==============================================================================

    HitTestMapper.cpp
    Author: Noah Baxter

    Inverts the forward rendering math in PositionMath.

  ==============================================================================
*/

#include "HitTestMapper.h"
#include "PositionConstants.h"
#include "BemaniConfig.h"

using namespace PositionConstants;

// =============================================================================
// Public
// =============================================================================

HitTestResult HitTestMapper::hitTest(float screenX, float screenY,
                                     uint viewportWidth, uint viewportHeight,
                                     double windowStartTime, double windowEndTime,
                                     bool isDrums, float farFadeEnd,
                                     float fretboardScale) const
{
    HitTestResult result;

    float position = invertYToPosition(screenY, viewportWidth, viewportHeight,
                                       isDrums, farFadeEnd);

    // Reject clicks outside the visible highway range
    if (position < HIGHWAY_POS_START || position > farFadeEnd)
        return result;

    double windowTimeSpan = windowEndTime - windowStartTime;
    result.normalizedPosition = position;
    result.timeFromCursor = (double)position * windowTimeSpan + windowStartTime;
    result.laneIndex = identifyLane(screenX, position, viewportWidth, viewportHeight,
                                    isDrums, fretboardScale);
    result.valid = true;

    return result;
}

// =============================================================================
// Bemani mode Y inversion (linear)
// =============================================================================

static float invertYBemani(float screenY, uint viewportHeight, float farFadeEnd)
{
    float strikePixelY = (float)viewportHeight * bemaniConfig.strikelinePos;
    float pixelsPerUnit = REFERENCE_HEIGHT * bemaniConfig.strikelinePos
                        / std::max(0.1f, farFadeEnd);

    // Forward: yPos = strikePixelY - position * pixelsPerUnit
    // Inverse: position = (strikePixelY - yPos) / pixelsPerUnit
    return (strikePixelY - screenY) / pixelsPerUnit;
}

// =============================================================================
// Perspective mode Y inversion
//
// Forward path (from createPerspectiveGlyphRect):
//   depth = position / vanishingPointDepth
//   k = 1/expMidProgress - 2
//   progress = (1 - depth) / (1 + depth * k)
//   adjNormY2 = normY2 + vanishingPointY
//   yPos = adjNormY2 * h + (normY1 - adjNormY2) * h * progress
//
// Solving for depth from yPos:
//   Let A = adjNormY2 * h, B = (normY1 - adjNormY2) * h
//   yPos = A + B * progress
//   progress = (yPos - A) / B
//   progress = (1 - depth) / (1 + depth * k)
//   depth = (1 - progress) / (progress * k + 1)
//   position = depth * vanishingPointDepth
// =============================================================================

static float invertYPerspective(float screenY, uint viewportWidth, uint viewportHeight,
                                bool isDrums, float farFadeEnd)
{
#ifdef DEBUG
    const auto& pp = PositionMath::perspParams(isDrums);
#else
    auto pp = getPerspectiveParams(isDrums);
#endif

    const auto& fbCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;

    // Compute k (same as forward path)
    float expDenom = std::pow(10.0f, pp.exponentialCurve) - 1.0f;
    float expMidProgress = (std::pow(10.0f, pp.exponentialCurve * 0.5f) - 1.0f) / expDenom;
    float k = 1.0f / expMidProgress - 2.0f;

    float adjNormY2 = fbCoords.normY2 + pp.vanishingPointY;
    float h = (float)viewportHeight;

    float A = adjNormY2 * h;
    float B = (fbCoords.normY1 - adjNormY2) * h;

    if (std::abs(B) < 0.001f)
        return -1.0f;  // degenerate

    float progress = (screenY - A) / B;

    // progress must be in (0, 1] for valid highway positions
    if (progress <= 0.0f || progress > 1.0f)
        return -1.0f;

    // Invert: depth = (1 - progress) / (progress * k + 1)
    float denom = progress * k + 1.0f;
    if (std::abs(denom) < 0.0001f)
        return -1.0f;

    float depth = (1.0f - progress) / denom;
    return depth * pp.vanishingPointDepth;
}

// =============================================================================
// Y inversion dispatch
// =============================================================================

float HitTestMapper::invertYToPosition(float screenY, uint viewportWidth, uint viewportHeight,
                                       bool isDrums, float farFadeEnd) const
{
    if (PositionMath::bemaniMode)
        return invertYBemani(screenY, viewportHeight, farFadeEnd);
    else
        return invertYPerspective(screenY, viewportWidth, viewportHeight, isDrums, farFadeEnd);
}

// =============================================================================
// Lane identification
// =============================================================================

int HitTestMapper::identifyLane(float screenX, float position,
                                uint viewportWidth, uint viewportHeight,
                                bool isDrums, float fretboardScale) const
{
    int numLanes = isDrums ? (int)DRUM_LANE_COUNT : (int)GUITAR_LANE_COUNT;
    const auto* laneCoords = isDrums ? drumBezierLaneCoords : guitarBezierLaneCoords;

    float bestDist = std::numeric_limits<float>::max();
    int bestLane = -1;

    for (int i = 0; i < numLanes; i++)
    {
        auto corners = PositionMath::getColumnPosition(
            isDrums, position, viewportWidth, viewportHeight,
            HIGHWAY_POS_START, HIGHWAY_POS_END,
            laneCoords[i],
            isDrums ? BAR_SIZE : GEM_SIZE,
            fretboardScale,
            PositionMath::bemaniMode ? i : -1);

        if (screenX >= corners.leftX && screenX <= corners.rightX)
            return i;  // Direct hit

        // Track nearest lane for close misses
        float center = (corners.leftX + corners.rightX) * 0.5f;
        float dist = std::abs(screenX - center);
        if (dist < bestDist)
        {
            bestDist = dist;
            bestLane = i;
        }
    }

    // Allow a small tolerance for near-misses (half a lane width)
    if (bestLane >= 0)
    {
        auto corners = PositionMath::getColumnPosition(
            isDrums, position, viewportWidth, viewportHeight,
            HIGHWAY_POS_START, HIGHWAY_POS_END,
            laneCoords[bestLane],
            isDrums ? BAR_SIZE : GEM_SIZE,
            fretboardScale,
            PositionMath::bemaniMode ? bestLane : -1);

        float laneWidth = corners.rightX - corners.leftX;
        if (bestDist < laneWidth * 0.5f)
            return bestLane;
    }

    return -1;
}
