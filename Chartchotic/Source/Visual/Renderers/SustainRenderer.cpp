/*
    ==============================================================================

        SustainRenderer.cpp
        Author:  Noah Baxter

        Sustain and lane rendering extracted from SceneRenderer.

    ==============================================================================
*/

#include "SustainRenderer.h"

using namespace PositionConstants;

SustainRenderer::SustainRenderer(juce::ValueTree& state, AssetManager& assetManager)
    : state(state), assetManager(assetManager)
{
}

void SustainRenderer::populate(DrawCallMap& drawCallMap, const TimeBasedSustainWindow& sustainWindow,
                               double windowStartTime, double windowEndTime,
                               uint width, uint height, bool showLanes, bool showSustains,
                               bool isPlaying,
                               float posEnd,
                               float farFadeEnd, float farFadeLen, float farFadeCurve,
                               const NormalizedCoordinates* laneCoordsGuitar,
                               const NormalizedCoordinates* laneCoordsDrums)
{
    currentDrawCallMap = &drawCallMap;
    this->width = width;
    this->height = height;
    this->showLanes = showLanes;
    this->showSustains = showSustains;
    this->isPlaying = isPlaying;
    this->posEnd = posEnd;
    this->farFadeEnd = farFadeEnd;
    this->farFadeLen = farFadeLen;
    this->farFadeCurve = farFadeCurve;
    this->laneCoordsGuitar = laneCoordsGuitar;
    this->laneCoordsDrums = laneCoordsDrums;

    for (const auto& sustain : sustainWindow)
    {
        drawSustain(sustain, windowStartTime, windowEndTime);
    }
}

void SustainRenderer::drawSustain(const TimeBasedSustainEvent& sustain, double windowStartTime, double windowEndTime)
{
    double windowTimeSpan = windowEndTime - windowStartTime;

    bool isLane = (sustain.sustainType == SustainType::LANE);

    // Gate by render toggle
    if (isLane && !showLanes) return;
    if (!isLane && !showSustains) return;

    bool hitAnimationsOn = state.getProperty("hitIndicators");

    // Determine clip position — only clip at strikeline during playback with hit animations.
    // When paused (even with hits enabled), show everything to support browsing/editing.
    bool clipAtStrike = hitAnimationsOn && isPlaying;
    float clipPos;
    if (isLane)
    {
        clipPos = HIGHWAY_POS_START;
    }
    else if (clipAtStrike)
    {
        clipPos = isBarNote(sustain.gemColumn, isGuitarLike(activePart) ? Part::GUITAR : Part::DRUMS)
            ? BAR_SUSTAIN_CLIP : SUSTAIN_CLIP;
    }
    else
    {
        clipPos = HIGHWAY_POS_START;
    }

    double clipTime = clipPos * windowTimeSpan;
    if (sustain.endTime < clipTime) return;

    double clippedStartTime = std::max(clipTime, sustain.startTime);

    // Position offsets differ for lanes vs sustains
    float startOffset, endOffset;
    if (isLane)
    {
        startOffset = laneShape.startOffset;
        endOffset = laneShape.endOffset;
    }
    else if (isBarNote(sustain.gemColumn, isGuitarLike(activePart) ? Part::GUITAR : Part::DRUMS))
    {
        startOffset = BAR_SUSTAIN_START_OFFSET;
        endOffset = BAR_SUSTAIN_END_OFFSET;
    }
    else
    {
        startOffset = SUSTAIN_START_OFFSET;
        endOffset = SUSTAIN_END_OFFSET;
    }

    // In Bemani mode, lanes use pixel-based padding (applied in drawSustainBody),
    // sustains still use position-space offsets
    if (PositionMath::bemaniMode)
    {
        if (sustain.sustainType == SustainType::LANE)
        {
            // Zero out position-space offsets — pixel padding applied after Y conversion
            startOffset = 0.0f;
            endOffset = 0.0f;
        }
        else
        {
            startOffset = bemaniConfig.sustStartOff;
            endOffset = bemaniConfig.sustEndOff;
        }
    }

    float startPosition = (float)((clippedStartTime - windowStartTime) / windowTimeSpan) + startOffset;
    float endPosition = (float)((sustain.endTime - windowStartTime) / windowTimeSpan) + endOffset;

    if (endPosition < clipPos || startPosition > farFadeEnd) return;

    startPosition = std::max(clipPos, startPosition);
    endPosition = std::min(farFadeEnd, endPosition);

    bool starPowerActive = state.getProperty("starPower");
    bool shouldBeWhite = starPowerActive && sustain.gemType.starPower;
    auto colour = assetManager.getLaneColour(sustain.gemColumn, isGuitarLike(activePart) ? Part::GUITAR : Part::DRUMS, shouldBeWhite);

    float opacity, sustainWidth;
    DrawOrder sustainDrawOrder;
    bool isKickCol = isDrumKick(sustain.gemColumn);
    switch (sustain.sustainType) {
        case SustainType::LANE:
            opacity = LANE_OPACITY;
            sustainWidth = isKickCol ? LANE_OPEN_WIDTH : LANE_WIDTH;
            sustainDrawOrder = DrawOrder::LANE;
            break;
        case SustainType::SUSTAIN:
        default:
            opacity = SUSTAIN_OPACITY;
            sustainWidth = isKickCol ? SUSTAIN_OPEN_WIDTH : SUSTAIN_WIDTH;
            sustainDrawOrder = isKickCol ? DrawOrder::BAR : DrawOrder::SUSTAIN;
            break;
    }

    // Look up lane coords for LanePainter
    bool isDrums = isDrumLike(activePart);
    NormalizedCoordinates colCoords;
    float laneScale;
    int bemaniIdx = -1;
    if (isDrums) {
        bool isKick = isDrumKick(sustain.gemColumn);
        uint dIdx = drumColumnIndex(sustain.gemColumn);
        colCoords = laneCoordsDrums[dIdx];
        laneScale = isKick ? PositionConstants::BAR_SIZE : PositionConstants::GEM_SIZE;
        bemaniIdx = (int)dIdx - 1;
    } else {
        colCoords = laneCoordsGuitar[sustain.gemColumn];
        laneScale = (sustain.gemColumn == 0) ? PositionConstants::BAR_SIZE : PositionConstants::GEM_SIZE;
        bemaniIdx = (int)sustain.gemColumn - 1;
    }

    LanePainter::Params lp {
        sustain.gemColumn, activePart,
        startPosition, endPosition,
        opacity, sustainWidth, colour, isLane,
        width, height, posEnd,
        colCoords, laneScale, bemaniIdx,
        laneShape,
        farFadeEnd, farFadeLen, farFadeCurve
    };

    (*currentDrawCallMap)[static_cast<int>(sustainDrawOrder)][sustain.gemColumn].push_back([lp](juce::Graphics& g) {
        LanePainter::paint(g, lp);
    });
}

