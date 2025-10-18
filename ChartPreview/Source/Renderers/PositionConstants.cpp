/*
    ==============================================================================

        PositionConstants.cpp
        Created by Claude Code (refactoring positioning logic)
        Author: Noah Baxter

        This file contains positioning data and lane coordinate calculations.
        All glyph positioning logic has been moved to GlyphRenderer.

    ==============================================================================
*/

#include "PositionConstants.h"

//==============================================================================
// Guitar Lane Coordinates (for column rendering)

PositionConstants::LaneCorners PositionConstants::getGuitarLaneCoordinates(uint gemColumn, float position, uint width, uint height)
{
    bool isOpen = (gemColumn == 0);
    auto coords = isOpen ? getGuitarLaneOpenCoords() : getGuitarLaneNoteCoords(gemColumn);

    float scaler = isOpen ? 0.95f : 0.9f;
    float scaledNormWidth1 = coords.normWidth1 * scaler;
    float scaledNormWidth2 = coords.normWidth2 * scaler;
    float adjustedNormX1 = coords.normX1 + (coords.normWidth1 - scaledNormWidth1) / 2.0f;
    float adjustedNormX2 = coords.normX2 + (coords.normWidth2 - scaledNormWidth2) / 2.0f;

    auto rect = createPerspectiveGlyphRect(position, coords.normY1, coords.normY2,
                                           adjustedNormX1, adjustedNormX2,
                                           scaledNormWidth1, scaledNormWidth2,
                                           isOpen, width, height);
    return {rect.getX(), rect.getRight(), rect.getCentreY()};
}

//==============================================================================
// Drum Lane Coordinates (for column rendering)

PositionConstants::LaneCorners PositionConstants::getDrumLaneCoordinates(uint gemColumn, float position, uint width, uint height)
{
    bool isKick = (gemColumn == 0 || gemColumn == 6);
    auto coords = isKick ? getDrumLaneKickCoords() : getDrumLanePadCoords(gemColumn);

    float scaler = isKick ? 0.95f : 0.9f;
    float scaledNormWidth1 = coords.normWidth1 * scaler;
    float scaledNormWidth2 = coords.normWidth2 * scaler;
    float adjustedNormX1 = coords.normX1 + (coords.normWidth1 - scaledNormWidth1) / 2.0f;
    float adjustedNormX2 = coords.normX2 + (coords.normWidth2 - scaledNormWidth2) / 2.0f;

    auto rect = createPerspectiveGlyphRect(position, coords.normY1, coords.normY2,
                                           adjustedNormX1, adjustedNormX2,
                                           scaledNormWidth1, scaledNormWidth2,
                                           isKick, width, height);
    return {rect.getX(), rect.getRight(), rect.getCentreY()};
}

//==============================================================================
// Core 3D Perspective Calculation (used for lane coordinates)

juce::Rectangle<float> PositionConstants::createPerspectiveGlyphRect(
    float position,
    float normY1, float normY2,
    float normX1, float normX2,
    float normWidth1, float normWidth2,
    bool isBarNote,
    uint width, uint height)
{
    auto perspParams = getPerspectiveParams();

    float depth = position;

    // Calculate 3D perspective scale for height
    float perspectiveScale = (perspParams.playerDistance + perspParams.highwayDepth * (1.0f - depth)) / perspParams.playerDistance;
    perspectiveScale = 1.0f + (perspectiveScale - 1.0f) * perspParams.perspectiveStrength;

    // Calculate dimensions
    float targetWidth = normWidth2 * width;
    float targetHeight = isBarNote ? targetWidth / perspParams.barNoteHeightRatio : targetWidth / perspParams.regularNoteHeightRatio;

    // Width calculation: both note types use exponential interpolation
    float widthProgress = (std::pow(10, perspParams.exponentialCurve * (1 - depth)) - 1) / (std::pow(10, perspParams.exponentialCurve) - 1);
    float interpolatedWidth = normWidth2 + (normWidth1 - normWidth2) * widthProgress;
    float finalWidth = interpolatedWidth * width;

    // Height uses perspective scaling for 3D effect
    float currentHeight = targetHeight * perspectiveScale;

    // Position calculation using exponential curve
    float progress = (std::pow(10, perspParams.exponentialCurve * (1 - depth)) - 1) / (std::pow(10, perspParams.exponentialCurve) - 1);
    float yPos = normY2 * height + (normY1 - normY2) * height * progress;
    float xPos = normX2 * width + (normX1 - normX2) * width * progress;

    // Apply X offset and center positioning
    float xOffset = targetWidth * perspParams.xOffsetMultiplier;
    float finalX = xPos + xOffset - targetWidth / 2.0f;
    float finalY = yPos - targetHeight / 2.0f;

    return juce::Rectangle<float>(finalX, finalY, finalWidth, currentHeight);
}

//==============================================================================
// Guitar Positioning Data (public access for GlyphRenderer)

PositionConstants::NormalizedCoordinates PositionConstants::getGuitarOpenNoteCoords()
{
    return {0.745f, 0.234f, 0.16f, 0.34f, 0.68f, 0.32f};
}

PositionConstants::NormalizedCoordinates PositionConstants::getGuitarNoteCoords(uint gemColumn)
{
    float normY1 = 0.73f;
    float normY2 = 0.22f;
    float normWidth1 = 0.125f;
    float normWidth2 = 0.065f;

    switch (gemColumn)
    {
        case 1:
            return {normY1, normY2, 0.20f, 0.363f, normWidth1, normWidth2};
        case 2:
            return {normY1, normY2, 0.320f, 0.412f, normWidth1, normWidth2};
        case 3:
            return {normY1, normY2, 0.440f, 0.465f, normWidth1, normWidth2};
        case 4:
            return {normY1, normY2, 0.557f, 0.524f, normWidth1, normWidth2};
        case 5:
            return {normY1, normY2, 0.673f, 0.580f, normWidth1, normWidth2};
        default:
            return {normY1, normY2, 0.20f, 0.363f, normWidth1, normWidth2};
    }
}

PositionConstants::NormalizedCoordinates PositionConstants::getDrumKickCoords()
{
    return {0.75f, 0.239f, 0.16f, 0.34f, 0.68f, 0.32f};
}

PositionConstants::NormalizedCoordinates PositionConstants::getDrumPadCoords(uint gemColumn)
{
    float normY1 = 0.72f;
    float normY2 = 0.22f;
    float normWidth1 = 0.147f;
    float normWidth2 = 0.0714f;

    switch (gemColumn)
    {
        case 1:
            return {normY1, normY2, 0.22f, 0.365f, normWidth1, normWidth2};
        case 2:
            return {normY1, normY2, 0.360f, 0.430f, normWidth1, normWidth2};
        case 3:
            return {normY1, normY2, 0.497f, 0.495f, normWidth1, normWidth2};
        case 4:
            return {normY1, normY2, 0.640f, 0.564f, normWidth1, normWidth2};
        default:
            return {normY1, normY2, 0.22f, 0.365f, normWidth1, normWidth2};
    }
}

//==============================================================================
// Guitar Lane Positioning Data

PositionConstants::NormalizedCoordinates PositionConstants::getGuitarLaneOpenCoords()
{
    return {0.73f, 0.234f, 0.16f, 0.34f, 0.68f, 0.32f};
}

PositionConstants::NormalizedCoordinates PositionConstants::getGuitarLaneNoteCoords(uint gemColumn)
{
    float normY1 = 0.71f;
    float normY2 = 0.22f;

    switch (gemColumn)
    {
        case 1:
            return {normY1, normY2, 0.227f, 0.363f, 0.105f, 0.055f};
        case 2:
            return {normY1, normY2, 0.322f, 0.412f, 0.125f, 0.065f};
        case 3:
            return {normY1, normY2, 0.440f, 0.465f, 0.125f, 0.065f};
        case 4:
            return {normY1, normY2, 0.555f, 0.524f, 0.125f, 0.065f};
        case 5:
            return {normY1, normY2, 0.670f, 0.580f, 0.125f, 0.065f};
        default:
            return {normY1, normY2, 0.227f, 0.363f, 0.105f, 0.055f};
    }
}

//==============================================================================
// Drum Lane Positioning Data

PositionConstants::NormalizedCoordinates PositionConstants::getDrumLaneKickCoords()
{
    return {0.735f, 0.239f, 0.16f, 0.34f, 0.68f, 0.32f};
}

PositionConstants::NormalizedCoordinates PositionConstants::getDrumLanePadCoords(uint gemColumn)
{
    float normY1 = 0.70f;
    float normY2 = 0.22f;
    float normWidth1 = 0.147f;
    float normWidth2 = 0.0714f;

    switch (gemColumn)
    {
        case 1:
            return {normY1, normY2, 0.222f, 0.37f, normWidth1, normWidth2};
        case 2:
            return {normY1, normY2, 0.360f, 0.430f, normWidth1, normWidth2};
        case 3:
            return {normY1, normY2, 0.497f, 0.495f, normWidth1, normWidth2};
        case 4:
            return {normY1, normY2, 0.630f, 0.564f, normWidth1, normWidth2};
        default:
            return {normY1, normY2, 0.222f, 0.37f, normWidth1, normWidth2};
    }
}

//==============================================================================
// Perspective Parameters

PositionConstants::PerspectiveParams PositionConstants::getPerspectiveParams() const
{
    return {
        100.0f,   // highwayDepth
        50.0f,    // playerDistance
        0.7f,     // perspectiveStrength
        0.5f,     // exponentialCurve
        0.5f,     // xOffsetMultiplier
        16.0f,    // barNoteHeightRatio
        2.0f      // regularNoteHeightRatio
    };
}
