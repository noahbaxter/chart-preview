/*
    ==============================================================================

        PositionMath.cpp
        Created by Claude Code (refactoring positioning logic)
        Author: Noah Baxter

        This file contains mathematical functions for computing glyph positions
        and lane coordinates using 3D perspective calculations.

    ==============================================================================
*/

#include "PositionMath.h"

using namespace PositionConstants;

//==============================================================================
// Static coordinate table array bounds
static constexpr size_t GUITAR_GLYPH_SIZE = 6;  // Open + 5 columns
static constexpr size_t DRUM_GLYPH_SIZE = 5;    // Kick + 4 columns

//==============================================================================
// Helper to apply scaling and centering to coordinates

NormalizedCoordinates PositionMath::applyWidthScaling(
    const NormalizedCoordinates& coords,
    float scaler)
{
    float scaledNormWidth1 = coords.normWidth1 * scaler;
    float scaledNormWidth2 = coords.normWidth2 * scaler;
    float adjustedNormX1 = coords.normX1 + (coords.normWidth1 - scaledNormWidth1) / 2.0f;
    float adjustedNormX2 = coords.normX2 + (coords.normWidth2 - scaledNormWidth2) / 2.0f;

    return {coords.normY1, coords.normY2, adjustedNormX1, adjustedNormX2, scaledNormWidth1, scaledNormWidth2};
}

//==============================================================================
// Generic lane coordinate lookup

const NormalizedCoordinates& PositionMath::lookupLaneCoords(
    const NormalizedCoordinates* coordTable,
    uint gemColumn)
{
    // All tables have column 0 (open/kick), so clamping to valid range
    return coordTable[gemColumn];
}

//==============================================================================
// Guitar Lane Coordinates (for column rendering)

LaneCorners PositionMath::getGuitarLaneCoordinates(uint gemColumn, float position, uint width, uint height)
{
    bool isOpen = (gemColumn == 0);
    const auto& baseCoords = lookupLaneCoords(guitarLaneCoords, gemColumn);

    float scaler = isOpen ? 0.95f : 0.9f;
    auto coords = applyWidthScaling(baseCoords, scaler);

    auto rect = createPerspectiveGlyphRect(position, coords.normY1, coords.normY2,
                                           coords.normX1, coords.normX2,
                                           coords.normWidth1, coords.normWidth2,
                                           isOpen, width, height);
    return {rect.getX(), rect.getRight(), rect.getCentreY()};
}

//==============================================================================
// Drum Lane Coordinates (for column rendering)

LaneCorners PositionMath::getDrumLaneCoordinates(uint gemColumn, float position, uint width, uint height)
{
    bool isKick = (gemColumn == 0 || gemColumn == 6);
    const auto& baseCoords = lookupLaneCoords(drumLaneCoords, gemColumn);

    float scaler = isKick ? 0.95f : 0.9f;
    auto coords = applyWidthScaling(baseCoords, scaler);

    auto rect = createPerspectiveGlyphRect(position, coords.normY1, coords.normY2,
                                           coords.normX1, coords.normX2,
                                           coords.normWidth1, coords.normWidth2,
                                           isKick, width, height);
    return {rect.getX(), rect.getRight(), rect.getCentreY()};
}

//==============================================================================
// Core 3D Perspective Calculation (used for lane coordinates)

juce::Rectangle<float> PositionMath::createPerspectiveGlyphRect(
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

NormalizedCoordinates PositionMath::getGuitarOpenNoteCoords()
{
    return PositionConstants::getGuitarOpenNoteCoords();
}

NormalizedCoordinates PositionMath::getGuitarNoteCoords(uint gemColumn)
{
    return PositionConstants::getGuitarNoteCoords(gemColumn);
}

NormalizedCoordinates PositionMath::getDrumKickCoords()
{
    return PositionConstants::getDrumKickCoords();
}

NormalizedCoordinates PositionMath::getDrumPadCoords(uint gemColumn)
{
    return PositionConstants::getDrumPadCoords(gemColumn);
}
