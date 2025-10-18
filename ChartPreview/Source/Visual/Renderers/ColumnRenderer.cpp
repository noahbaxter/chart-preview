/*
    ==============================================================================

        ColumnRenderer.cpp
        Created by Claude Code (refactoring column/sustain rendering logic)
        Author: Noah Baxter

        This file contains all column and sustain path generation logic.
        Handles rendering of vertical elements like sustains, BREs, sections, etc.

    ==============================================================================
*/

#include "ColumnRenderer.h"

//==============================================================================
// Trapezoid Path Generation

juce::Path ColumnRenderer::createTrapezoidPath(LaneCorners start, LaneCorners end, float startWidth, float endWidth)
{
    juce::Path path;
    float startCenterX = (start.leftX + start.rightX) / 2.0f;
    float endCenterX = (end.leftX + end.rightX) / 2.0f;

    path.startNewSubPath(startCenterX - startWidth / 2.0f, start.centerY);
    path.lineTo(startCenterX + startWidth / 2.0f, start.centerY);
    path.lineTo(endCenterX + endWidth / 2.0f, end.centerY);
    path.lineTo(endCenterX - endWidth / 2.0f, end.centerY);
    path.closeSubPath();

    return path;
}

//==============================================================================
// Rounded Cap Path Generation

juce::Path ColumnRenderer::createRoundedCapPath(LaneCorners coords, float width, float radius, float heightScale)
{
    juce::Path path;
    float centerX = (coords.leftX + coords.rightX) / 2.0f;
    float capHeight = radius * 2.0f * heightScale;

    path.addRoundedRectangle(
        centerX - width / 2.0f,
        coords.centerY - capHeight / 2.0f,
        width,
        capHeight,
        radius * heightScale
    );

    return path;
}

//==============================================================================
// Offscreen Sustain Image Rendering

juce::Image ColumnRenderer::createOffscreenColumnImage(
    const juce::Path& trapezoid,
    const juce::Path& startCap,
    const juce::Path& endCap,
    juce::Colour colour)
{
    auto bounds = trapezoid.getBounds().getUnion(startCap.getBounds()).getUnion(endCap.getBounds());
    int width = (int)std::ceil(bounds.getWidth()) + 2;
    int height = (int)std::ceil(bounds.getHeight()) + 2;

    juce::Image image(juce::Image::ARGB, width, height, true);
    juce::Graphics graphics(image);

    // Translate to local coordinates
    graphics.addTransform(juce::AffineTransform::translation(-bounds.getX() + 1, -bounds.getY() + 1));
    graphics.setColour(colour);

    // Draw trapezoid, then caps with clipping to avoid overlap
    graphics.fillPath(trapezoid);
    graphics.excludeClipRegion(trapezoid.getBounds().toNearestInt());
    graphics.fillPath(startCap);
    graphics.fillPath(endCap);

    return image;
}
