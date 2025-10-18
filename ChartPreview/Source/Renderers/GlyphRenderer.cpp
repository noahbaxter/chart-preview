/*
    ==============================================================================

        GlyphRenderer.cpp
        Created by Claude Code (refactoring positioning logic)
        Author: Noah Baxter

        This file contains glyph (note) positioning and rendering logic.
        Uses PositionConstants for all positioning data.

    ==============================================================================
*/

#include "GlyphRenderer.h"

//==============================================================================
// Guitar Glyph Positioning

juce::Rectangle<float> GlyphRenderer::getGuitarGlyphRect(uint gemColumn, float position, uint width, uint height)
{
    bool isOpen = (gemColumn == 0);
    auto coords = isOpen ? positionConstants.getGuitarOpenNoteCoords() : positionConstants.getGuitarNoteCoords(gemColumn);

    float scaler = isOpen ? 0.95f : 0.9f;  // BAR_SIZE : GEM_SIZE
    float scaledNormWidth1 = coords.normWidth1 * scaler;
    float scaledNormWidth2 = coords.normWidth2 * scaler;
    float adjustedNormX1 = coords.normX1 + (coords.normWidth1 - scaledNormWidth1) / 2.0f;
    float adjustedNormX2 = coords.normX2 + (coords.normWidth2 - scaledNormWidth2) / 2.0f;

    return createPerspectiveGlyphRect(position, coords.normY1, coords.normY2,
                                      adjustedNormX1, adjustedNormX2,
                                      scaledNormWidth1, scaledNormWidth2,
                                      isOpen, width, height);
}

juce::Rectangle<float> GlyphRenderer::getGuitarGridlineRect(float position, uint width, uint height)
{
    auto coords = positionConstants.getGuitarOpenNoteCoords();
    float scaler = 0.9f;  // GRIDLINE_SIZE

    float scaledNormWidth1 = coords.normWidth1 * scaler;
    float scaledNormWidth2 = coords.normWidth2 * scaler;
    float adjustedNormX1 = coords.normX1 + (coords.normWidth1 - scaledNormWidth1) / 2.0f;
    float adjustedNormX2 = coords.normX2 + (coords.normWidth2 - scaledNormWidth2) / 2.0f;

    return createPerspectiveGlyphRect(position, coords.normY1, coords.normY2,
                                      adjustedNormX1, adjustedNormX2,
                                      scaledNormWidth1, scaledNormWidth2,
                                      true, width, height);
}

//==============================================================================
// Drum Glyph Positioning

juce::Rectangle<float> GlyphRenderer::getDrumGlyphRect(uint gemColumn, float position, uint width, uint height)
{
    bool isKick = (gemColumn == 0 || gemColumn == 6);
    auto coords = isKick ? positionConstants.getDrumKickCoords() : positionConstants.getDrumPadCoords(gemColumn);

    float scaler = isKick ? 0.95f : 0.9f;  // BAR_SIZE : GEM_SIZE
    float scaledNormWidth1 = coords.normWidth1 * scaler;
    float scaledNormWidth2 = coords.normWidth2 * scaler;
    float adjustedNormX1 = coords.normX1 + (coords.normWidth1 - scaledNormWidth1) / 2.0f;
    float adjustedNormX2 = coords.normX2 + (coords.normWidth2 - scaledNormWidth2) / 2.0f;

    return createPerspectiveGlyphRect(position, coords.normY1, coords.normY2,
                                      adjustedNormX1, adjustedNormX2,
                                      scaledNormWidth1, scaledNormWidth2,
                                      isKick, width, height);
}

juce::Rectangle<float> GlyphRenderer::getDrumGridlineRect(float position, uint width, uint height)
{
    auto coords = positionConstants.getDrumKickCoords();
    float scaler = 0.9f;  // GRIDLINE_SIZE

    float scaledNormWidth1 = coords.normWidth1 * scaler;
    float scaledNormWidth2 = coords.normWidth2 * scaler;
    float adjustedNormX1 = coords.normX1 + (coords.normWidth1 - scaledNormWidth1) / 2.0f;
    float adjustedNormX2 = coords.normX2 + (coords.normWidth2 - scaledNormWidth2) / 2.0f;

    return createPerspectiveGlyphRect(position, coords.normY1, coords.normY2,
                                      adjustedNormX1, adjustedNormX2,
                                      scaledNormWidth1, scaledNormWidth2,
                                      true, width, height);
}

//==============================================================================
// Overlay Positioning

juce::Rectangle<float> GlyphRenderer::getOverlayGlyphRect(juce::Rectangle<float> glyphRect, bool isDrumAccent)
{
    if (isDrumAccent)
    {
        float scaleFactor = 1.1232876712f * 0.9f;  // 1.1232876712 * GEM_SIZE
        float newWidth = glyphRect.getWidth() * scaleFactor;
        float newHeight = glyphRect.getHeight() * scaleFactor;
        float widthIncrease = newWidth - glyphRect.getWidth();
        float heightIncrease = newHeight - glyphRect.getHeight();

        float xPos = glyphRect.getX() - widthIncrease / 2;
        float yPos = glyphRect.getY() - heightIncrease / 2;

        return juce::Rectangle<float>(xPos, yPos, newWidth, newHeight);
    }

    return glyphRect;
}

//==============================================================================
// Core 3D Perspective Calculation

juce::Rectangle<float> GlyphRenderer::createPerspectiveGlyphRect(
    float position,
    float normY1, float normY2,
    float normX1, float normX2,
    float normWidth1, float normWidth2,
    bool isBarNote,
    uint width, uint height)
{
    auto perspParams = positionConstants.getPerspectiveParams();

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
