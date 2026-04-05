/*
    ==============================================================================

        GridlinePainter.cpp
        Author:  Noah Baxter

        Stateless gridline drawing. Extracted from GridlineRenderer.

    ==============================================================================
*/

#include "GridlinePainter.h"

using namespace PositionConstants;

namespace GridlinePainter
{

void paintBemani(juce::Graphics& g, const Params& p)
{
    if (!p.markerImage) return;

    bool isDrums = isDrumLike(p.activePart);
    auto edge = PositionMath::getFretboardEdge(isDrums, p.position, p.viewportW, p.viewportH,
                    HIGHWAY_POS_START, p.posEnd);
    float lineH = std::max(1.0f, (float)p.viewportW * 0.003f);
    float lineY = edge.centerY - lineH * 0.5f + bemaniConfig.gridlineZ;

    // Gradient: brighter at center, fades at edges
    juce::ColourGradient grad(
        juce::Colours::white.withAlpha(p.opacity), (edge.leftX + edge.rightX) * 0.5f, lineY,
        juce::Colours::white.withAlpha(p.opacity * 0.3f), edge.leftX, lineY, false);
    grad.addColour(0.0, juce::Colours::white.withAlpha(p.opacity * 0.3f));
    grad.addColour(0.5, juce::Colours::white.withAlpha(p.opacity));
    grad.addColour(1.0, juce::Colours::white.withAlpha(p.opacity * 0.3f));
    g.setGradientFill(grad);
    g.fillRect(edge.leftX, lineY, edge.rightX - edge.leftX, lineH);
}

void paintPerspective(juce::Graphics& g, const Params& p)
{
    if (!p.markerImage) return;

    bool isDrums = isDrumLike(p.activePart);
    const auto& fbCoords = isDrums
        ? drumFretboardCoords
        : guitarFretboardCoords;
    auto edge = PositionMath::getColumnPosition(isDrums, p.position, p.viewportW, p.viewportH,
                                                 HIGHWAY_POS_START, p.posEnd,
                                                 fbCoords, GRIDLINE_WIDTH_SCALE);
    float gridWidth = edge.rightX - edge.leftX;
    auto perspParams = getPerspectiveParams(isDrums);
    float gridHeight = gridWidth / perspParams.barNoteHeightRatio;

    // Scale Z offset by perspective (ratio of current width to strikeline width)
    float scaledZOffset = p.gridZOffset;
    if (std::abs(p.gridZOffset) > 0.001f)
    {
        auto strikeEdge = PositionMath::getColumnPosition(isDrums, 0.0f, p.viewportW, p.viewportH,
                                                           HIGHWAY_POS_START, p.posEnd,
                                                           fbCoords, GRIDLINE_WIDTH_SCALE);
        float strikeWidth = strikeEdge.rightX - strikeEdge.leftX;
        if (strikeWidth > 0.0f)
            scaledZOffset *= (gridWidth / strikeWidth);
    }

    juce::Rectangle<float> rect(edge.leftX, edge.centerY - gridHeight * 0.5f + scaledZOffset, gridWidth, gridHeight);
    g.setOpacity(p.opacity);
    g.drawImage(*p.markerImage, rect);
}

} // namespace GridlinePainter
