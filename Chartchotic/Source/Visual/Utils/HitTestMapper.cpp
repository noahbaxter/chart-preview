/*
  ==============================================================================

    HitTestMapper.cpp
    Author: Noah Baxter

    Inverts the forward rendering math in PositionMath.

  ==============================================================================
*/

#include "HitTestMapper.h"
#include "PositionConstants.h"
#include "../../Utils/ChartTypes.h"
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

    // Clamp at the visible highway bottom (HIGHWAY_POS_START).
    // Negative positions are valid — they're "in front of" the strikeline.
    // PPQ floor at 0 is applied downstream when converting to absolute time.
    if (position < HIGHWAY_POS_START)
        position = HIGHWAY_POS_START;

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
//
// Forward/inverse divergence above the vanishing-point line (yPos < A):
//   The forward path compresses position 1..farFadeEnd into a tiny screen
//   strip ([0, A] — typically ~2px). The asymptotic inverse can technically
//   resolve clicks in that strip back to position > 1, but the user can't
//   reliably target 2 pixels. Instead, we linearly interpolate position
//   from 1.0 (at progress=0) to farFadeEnd (at the viewport top edge,
//   progress = -A/B). Forward visuals stay perspective-accurate; the click
//   path gives the user a usable click target in the extended zone.
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

    // Invert: depth = (1 - progress) / (progress * k + 1)
    // Works for the full range:
    //   progress > 0  → normal highway (position 0→~1.0)
    //   progress < 0  → above VP line (position > 1.0, grows as cursor moves up)
    //   progress < -1/k → past singularity (clamp to farFadeEnd)
    float denom = progress * k + 1.0f;
    if (std::abs(denom) < 0.0001f)
        return farFadeEnd;

    float depth = (1.0f - progress) / denom;

    // Below strikeline (progress > 1 → negative depth): valid for authoring
    if (depth < 0.0f && progress > 1.0f)
        return depth * pp.vanishingPointDepth;

    // Past singularity (negative depth, progress < 0)
    if (depth < 0.0f)
        return farFadeEnd;

    return std::min(depth * pp.vanishingPointDepth, farFadeEnd);
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

    // Outside fretboard — left = open/kick, right = last lane (or 2x kick)
    auto fbEdge = PositionMath::getFretboardEdge(
        isDrums, position, viewportWidth, viewportHeight,
        HIGHWAY_POS_START, HIGHWAY_POS_END);

    if (screenX < fbEdge.leftX)
        return 0;

    if (screenX > fbEdge.rightX)
    {
        if (isDrums)
            return DRUM_KICK_2X_COLUMN;
        return (int)GUITAR_LANE_COUNT - 1;
    }

    // Right-to-left: left of lane N's left edge = lane N-1, etc.
    // Left of lane 1's left edge = open/kick (lane 0).
    for (int i = numLanes - 1; i >= 1; i--)
    {
        auto corners = PositionMath::getColumnPosition(
            isDrums, position, viewportWidth, viewportHeight,
            HIGHWAY_POS_START, HIGHWAY_POS_END,
            laneCoords[i], GEM_SIZE, fretboardScale,
            PositionMath::bemaniMode ? i : -1);
        if (screenX >= corners.leftX)
            return i;
    }

    return 0;
}
