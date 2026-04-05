/*
    ==============================================================================

        LanePainter.cpp
        Author:  Noah Baxter

        Stateless sustain/lane polygon drawing. Extracted from SustainRenderer.

    ==============================================================================
*/

#include "LanePainter.h"

using namespace PositionConstants;

namespace LanePainter
{

static LaneCorners getColumnEdge(bool isDrums, float position, uint width, uint height,
                                 float posEnd, const NormalizedCoordinates& colCoords,
                                 float sizeScale, float fretboardScale = 1.0f,
                                 int bemaniLaneIdx = -1)
{
    return PositionMath::getColumnPosition(isDrums, position, width, height,
                                           HIGHWAY_POS_START, posEnd,
                                           colCoords, sizeScale, fretboardScale, bemaniLaneIdx);
}

void paint(juce::Graphics& g, const Params& p)
{
    bool isDrums = isDrumLike(p.activePart);
    bool isBar = isBarNote(p.gemColumn, isDrums ? Part::DRUMS : Part::GUITAR);

    int laneIdx = PositionMath::bemaniMode ? p.bemaniLaneIdx : -1;
    auto startLane = getColumnEdge(isDrums, p.startPosition, p.viewportW, p.viewportH,
                                   p.posEnd, p.laneCoords, p.laneScale, FRETBOARD_SCALE, laneIdx);
    auto endLane = getColumnEdge(isDrums, p.endPosition, p.viewportW, p.viewportH,
                                 p.posEnd, p.laneCoords, p.laneScale, FRETBOARD_SCALE, laneIdx);

    // Bemani mode: simple shapes, no perspective geometry
    if (PositionMath::bemaniMode)
    {
        float bSustW = bemaniConfig.sustainWidth;
        float bBarSustW = bemaniConfig.barSustainWidth;
        float capFrac = bemaniConfig.sustainCap;

        float laneWidth = startLane.rightX - startLane.leftX;
        float centerX = (startLane.leftX + startLane.rightX) * 0.5f;
        float topY = std::min(startLane.centerY, endLane.centerY);
        float botY = std::max(startLane.centerY, endLane.centerY);

        float zNudge = p.isLane ? bemaniConfig.laneZ : bemaniConfig.sustainZ;
        topY += zNudge;
        botY += zNudge;

        // Pixel-based lane padding — extends lane past note edges by fixed pixels
        if (p.isLane)
        {
            float laneEndPxVal = isBar ? bemaniConfig.barLaneEndPx(isDrums) : bemaniConfig.laneEndPx(isDrums);
            topY -= laneEndPxVal;
            botY += isBar ? bemaniConfig.barLaneStartPx : bemaniConfig.laneStartPx;
        }

        if (isBar)
        {
            auto fb = PositionMath::getFretboardEdge(isDrums, p.startPosition, p.viewportW, p.viewportH,
                HIGHWAY_POS_START, p.posEnd);
            centerX = (fb.leftX + fb.rightX) * 0.5f;
            laneWidth = fb.rightX - fb.leftX;
        }

        if (p.isLane)
        {
            // Lanes: wider fill with caps on both ends
            float w = isBar ? laneWidth * bemaniConfig.barLaneFillW : laneWidth * bemaniConfig.laneFillW;
            float lx = centerX - w * 0.5f;
            float laneCapFrac = isBar ? bemaniConfig.barCap : bemaniConfig.laneCap;
            float capH = laneCapFrac * w;

            g.setColour(p.colour.withAlpha(p.opacity));
            juce::Path path;
            path.startNewSubPath(lx, topY);
            path.quadraticTo(centerX, topY - capH, lx + w, topY);
            path.lineTo(lx + w, botY);
            path.quadraticTo(centerX, botY + capH, lx, botY);
            path.closeSubPath();
            g.fillPath(path);
        }
        else
        {
            // Sustains: narrower, no front cap, small back cap (bar sustains get less cap)
            float w = isBar ? laneWidth * bBarSustW * 0.6f : laneWidth * bSustW;
            float lx = centerX - w * 0.5f;
            float sustCapFrac = isBar ? bemaniConfig.barCap : capFrac;
            float backCapH = sustCapFrac * w * 0.3f;

            g.setColour(p.colour.withAlpha(p.opacity));
            juce::Path path;
            // Top (far end) — small back cap
            path.startNewSubPath(lx, topY);
            if (backCapH > 0.5f)
                path.quadraticTo(centerX, topY - backCapH, lx + w, topY);
            else
                path.lineTo(lx + w, topY);
            // Right side down
            path.lineTo(lx + w, botY);
            // Bottom (strikeline end) — flat, no cap
            path.lineTo(lx, botY);
            path.closeSubPath();
            g.fillPath(path);
        }
        return;
    }

    float startWidth = (startLane.rightX - startLane.leftX) * p.sustainWidth;
    float endWidth = (endLane.rightX - endLane.leftX) * p.sustainWidth;

    float startCenterX = (startLane.leftX + startLane.rightX) * 0.5f;
    float endCenterX = (endLane.leftX + endLane.rightX) * 0.5f;

    float startLeftX  = startCenterX - startWidth * 0.5f;
    float startRightX = startCenterX + startWidth * 0.5f;
    float endLeftX    = endCenterX - endWidth * 0.5f;
    float endRightX   = endCenterX + endWidth * 0.5f;

    float startY = startLane.centerY;
    float endY   = endLane.centerY;

    float startCurve, endCurve;
    if (p.isLane)
    {
        startCurve = p.laneShape.innerStartArc;
        endCurve = p.laneShape.innerEndArc;
    }
    else if (isBar)
    {
        startCurve = BAR_SUSTAIN_START_CURVE;
        endCurve = BAR_SUSTAIN_END_CURVE;
    }
    else
    {
        startCurve = SUSTAIN_START_CURVE;
        endCurve = SUSTAIN_END_CURVE;
    }

    const auto& fbCoords = isDrums
        ? drumFretboardCoords
        : guitarFretboardCoords;
    auto startFretboard = getColumnEdge(isDrums, p.startPosition, p.viewportW, p.viewportH,
                                        p.posEnd, fbCoords, 1.0f);
    auto endFretboard = getColumnEdge(isDrums, p.endPosition, p.viewportW, p.viewportH,
                                      p.posEnd, fbCoords, 1.0f);

    float startFretboardWidth = startFretboard.rightX - startFretboard.leftX;
    float endFretboardWidth = endFretboard.rightX - endFretboard.leftX;

    float startArcY = startCurve * startFretboardWidth;
    float endArcY = endCurve * endFretboardWidth;

    float laneStartParabolaY = 0.0f;
    float laneEndParabolaY = 0.0f;
    if (p.isLane)
    {
        float startT = (startFretboardWidth > 0.0f) ? (startCenterX - startFretboard.leftX) / startFretboardWidth : 0.5f;
        float endT = (endFretboardWidth > 0.0f) ? (endCenterX - endFretboard.leftX) / endFretboardWidth : 0.5f;

        laneStartParabolaY = p.laneShape.outerStartArc * startFretboardWidth * 4.0f * startT * (1.0f - startT);
        laneEndParabolaY = p.laneShape.outerEndArc * endFretboardWidth * 4.0f * endT * (1.0f - endT);
    }

    juce::Path path;

    float adjStartY = startY + laneStartParabolaY;
    float adjEndY = endY + laneEndParabolaY;

    path.startNewSubPath(startLeftX, adjStartY);

    float startMidX = (startLeftX + startRightX) * 0.5f;
    path.quadraticTo(startMidX, adjStartY + startArcY, startRightX, adjStartY);

    if (p.isLane && std::abs(LANE_SIDE_CURVE) > 0.001f)
    {
        float sideMidY = (adjStartY + adjEndY) * 0.5f;
        float sideArc = LANE_SIDE_CURVE * (startFretboardWidth + endFretboardWidth) * 0.5f;
        path.quadraticTo(startRightX + sideArc, sideMidY, endRightX, adjEndY);
    }
    else
    {
        path.lineTo(endRightX, adjEndY);
    }

    float endMidX = (endLeftX + endRightX) * 0.5f;
    path.quadraticTo(endMidX, adjEndY + endArcY, endLeftX, adjEndY);

    if (p.isLane && std::abs(LANE_SIDE_CURVE) > 0.001f)
    {
        float sideMidY = (adjStartY + adjEndY) * 0.5f;
        float sideArc = LANE_SIDE_CURVE * (startFretboardWidth + endFretboardWidth) * 0.5f;
        path.quadraticTo(endLeftX - sideArc, sideMidY, startLeftX, adjStartY);
    }
    else
    {
        path.closeSubPath();
    }

    // Fill the path — use gradient if sustain extends into the fade zone
    float fadeStart = p.farFadeEnd - p.farFadeLen;
    if (p.endPosition > fadeStart)
    {
        float fadeStartClamped = std::max(fadeStart, p.startPosition);
        auto fadeStartEdge = PositionMath::getFretboardEdge(isDrums, fadeStartClamped, p.viewportW, p.viewportH,
            HIGHWAY_POS_START, p.posEnd);
        auto fadeEndEdge = PositionMath::getFretboardEdge(isDrums, p.farFadeEnd, p.viewportW, p.viewportH,
            HIGHWAY_POS_START, p.posEnd);

        float gradStartY = fadeStartEdge.centerY;
        float gradEndY   = fadeEndEdge.centerY;

        float startOpacity = calculateFarFade(fadeStartClamped, p.farFadeEnd, p.farFadeLen, p.farFadeCurve) * p.opacity;

        juce::ColourGradient gradient(
            p.colour.withAlpha(startOpacity), 0.0f, gradStartY,
            p.colour.withAlpha(0.0f), 0.0f, gradEndY,
            false);

        if (p.startPosition < fadeStart)
        {
            float fullRange = adjStartY - gradEndY;
            float opaqueRange = adjStartY - gradStartY;
            float opaqueT = (fullRange > 0.0f) ? opaqueRange / fullRange : 0.0f;

            gradient = juce::ColourGradient(
                p.colour.withAlpha(p.opacity), 0.0f, adjStartY,
                p.colour.withAlpha(0.0f), 0.0f, gradEndY,
                false);
            gradient.addColour(opaqueT, p.colour.withAlpha(p.opacity));
        }

        g.setGradientFill(gradient);
    }
    else
    {
        g.setColour(p.colour.withAlpha(p.opacity));
    }
    g.fillPath(path);
}

} // namespace LanePainter
