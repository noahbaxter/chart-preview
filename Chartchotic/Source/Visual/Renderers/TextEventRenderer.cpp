/*
    ==============================================================================

        TextEventRenderer.cpp
        Author:  Noah Baxter

    ==============================================================================
*/

#include "TextEventRenderer.h"

using namespace PositionConstants;

// How much the top/bottom edges bow downward (fraction of band width)
static constexpr float EDGE_CURVATURE = -0.025f;

void TextEventRenderer::populate(DrawCallMap& drawCallMap,
                                 const std::vector<TimeBasedFlipRegion>& flipRegions,
                                 double windowStartTime, double windowEndTime,
                                 uint width, uint height,
                                 float posEnd,
                                 float farFadeEnd, float farFadeLen, float farFadeCurve)
{
    setFrame(activePart, width, height, posEnd);

    double windowTimeSpan = windowEndTime - windowStartTime;
    if (windowTimeSpan <= 0.0) return;

    for (const auto& region : flipRegions)
    {
        // Start marker
        float startPos = (float)((region.startTime - windowStartTime) / windowTimeSpan);
        if (startPos >= HIGHWAY_POS_START && startPos <= farFadeEnd)
        {
            float fade = calculateFarFade(startPos, farFadeEnd, farFadeLen, farFadeCurve);
            drawCallMap[static_cast<int>(DrawOrder::TEXT_EVENT)][0].push_back(
                [this, startPos, fade](juce::Graphics& g) {
                    this->drawMarker(g, startPos, "DISCO START", fade);
                });
        }

        // End marker — no lower bound so it scrolls past the strikeline
        float endPos = (float)((region.endTime - windowStartTime) / windowTimeSpan);
        if (endPos <= farFadeEnd)
        {
            float fade = calculateFarFade(endPos, farFadeEnd, farFadeLen, farFadeCurve);
            drawCallMap[static_cast<int>(DrawOrder::TEXT_EVENT)][0].push_back(
                [this, endPos, fade](juce::Graphics& g) {
                    this->drawMarker(g, endPos, "DISCO END", fade);
                });
        }
    }
}

void TextEventRenderer::populateEventMarkers(DrawCallMap& drawCallMap,
                                              const TimeBasedEventMarkers& markers,
                                              double windowStartTime, double windowEndTime,
                                              uint width, uint height,
                                              float posEnd,
                                              float farFadeEnd, float farFadeLen, float farFadeCurve)
{
    setFrame(activePart, width, height, posEnd);

    double windowTimeSpan = windowEndTime - windowStartTime;
    if (windowTimeSpan <= 0.0) return;

    for (const auto& marker : markers)
    {
        float pos = (float)((marker.time - windowStartTime) / windowTimeSpan);
        if (pos >= HIGHWAY_POS_START && pos <= farFadeEnd)
        {
            float fade = calculateFarFade(pos, farFadeEnd, farFadeLen, farFadeCurve);
            juce::String label = marker.label;
            drawCallMap[static_cast<int>(DrawOrder::TEXT_EVENT)][0].push_back(
                [this, pos, label, fade](juce::Graphics& g) {
                    this->drawMarker(g, pos, label, fade);
                });
        }
    }
}

void TextEventRenderer::drawMarker(juce::Graphics& g, float position, const juce::String& label,
                                   float fadeOpacity)
{
    if (PositionMath::bemaniMode)
    {
        // Flat horizontal marker matching Bemani gridline style
        auto edge = PositionMath::getFretboardEdge(getRenderType(activePart), position, width, height,
                        PositionConstants::HIGHWAY_POS_START, posEnd);
        float leftX = edge.leftX;
        float rightX = edge.rightX;
        float bandWidth = rightX - leftX;
        float baseY = edge.centerY;

        // Band dimensions
        float bandH = std::max(24.0f, bandWidth * 0.10f);
        float topY = baseY - bandH;
        float lineH = std::max(2.0f, bandH * 0.12f);

        // Translucent band fill
        g.setColour(juce::Colours::white.withAlpha(0.12f * fadeOpacity));
        g.fillRect(leftX, topY, bandWidth, bandH);

        // Bright base line (bottom edge of band)
        g.setColour(juce::Colours::white.withAlpha(0.8f * fadeOpacity));
        g.fillRect(leftX, baseY - lineH, bandWidth, lineH);

        // Dimmer top line
        g.setColour(juce::Colours::white.withAlpha(0.4f * fadeOpacity));
        g.fillRect(leftX, topY, bandWidth, std::max(1.0f, lineH * 0.5f));

        // Text label centered in band
        g.setColour(juce::Colours::white.withAlpha(0.95f * fadeOpacity));
        float fontSize = std::max(11.0f, bandH * 0.55f);
        g.setFont(juce::Font(fontSize, juce::Font::bold));
        g.drawText(label, juce::Rectangle<float>(leftX, topY, bandWidth, bandH),
                   juce::Justification::centred, false);
        return;
    }

    // --- Perspective mode: curved force field ---

    // Width from fretboard coords, same scale as gridlines
    const auto& fbCoords = *currentConfig->fretboardCoords;
    auto laneEdge = getColumnEdge(position, fbCoords, TEXT_EVENT_WIDTH_SCALE);
    float leftX  = laneEdge.leftX;
    float rightX = laneEdge.rightX;
    float baseY  = laneEdge.centerY;
    float bandWidth = rightX - leftX;

    // Wall height proportional to width (perspective-correct)
    float wallHeight = bandWidth * 0.28f;
    float topY = baseY - wallHeight;

    // Curvature: subtle downward bow on top and bottom edges
    float curvePx = bandWidth * EDGE_CURVATURE;
    float midX = (leftX + rightX) * 0.5f;

    // --- Build the force field shape ---
    // Left side (straight up), top edge (curved), right side (straight down), bottom edge (curved)
    juce::Path wall;
    wall.startNewSubPath(leftX, baseY);
    wall.lineTo(leftX, topY);
    // Top edge: quadratic bezier bowing downward
    wall.quadraticTo(midX, topY + curvePx, rightX, topY);
    wall.lineTo(rightX, baseY);
    // Bottom edge: quadratic bezier bowing downward
    wall.quadraticTo(midX, baseY + curvePx, leftX, baseY);
    wall.closeSubPath();

    // Gradient fill: brighter at base, fading toward top
    juce::ColourGradient gradient(
        juce::Colours::white.withAlpha(0.45f * fadeOpacity), 0.0f, baseY,
        juce::Colours::white.withAlpha(0.10f * fadeOpacity), 0.0f, topY,
        false);
    g.setGradientFill(gradient);
    g.fillPath(wall);

    // Base edge line (bright, sits on highway surface, curved)
    float lineThick = juce::jmax(2.0f, wallHeight * 0.07f);
    g.setColour(juce::Colours::white.withAlpha(0.9f * fadeOpacity));
    juce::Path baseLine;
    baseLine.startNewSubPath(leftX, baseY);
    baseLine.quadraticTo(midX, baseY + curvePx, rightX, baseY);
    g.strokePath(baseLine, juce::PathStrokeType(lineThick));

    // Top edge line (thinner, curved)
    float topLineThick = juce::jmax(1.0f, lineThick * 0.5f);
    g.setColour(juce::Colours::white.withAlpha(0.5f * fadeOpacity));
    juce::Path topLine;
    topLine.startNewSubPath(leftX, topY);
    topLine.quadraticTo(midX, topY + curvePx, rightX, topY);
    g.strokePath(topLine, juce::PathStrokeType(topLineThick));

    // Side edge lines (straight vertical)
    float sideThick = juce::jmax(1.0f, lineThick * 0.4f);
    g.setColour(juce::Colours::white.withAlpha(0.35f * fadeOpacity));
    g.drawLine(leftX, baseY, leftX, topY, sideThick);
    g.drawLine(rightX, baseY, rightX, topY, sideThick);

    // Text label centered in the wall
    g.setColour(juce::Colours::white.withAlpha(0.95f * fadeOpacity));
    float fontSize = juce::jmax(12.0f, wallHeight * 0.32f);
    g.setFont(juce::Font(fontSize, juce::Font::bold));
    juce::Rectangle<float> textRect(leftX, topY, bandWidth, wallHeight);
    g.drawText(label, textRect, juce::Justification::centred, false);
}
