/*
    ==============================================================================

        GridlineRenderer.cpp
        Author:  Noah Baxter

        Gridline rendering extracted from SceneRenderer.

    ==============================================================================
*/

#include "GridlineRenderer.h"
#include "../Utils/RenderTypeConfig.h"
#include "../Utils/Frame.h"
#include "../Utils/FrameRenderer.h"

using namespace PositionConstants;
using namespace Render;

GridlineRenderer::GridlineRenderer(juce::ValueTree& state, AssetManager& assetManager)
    : state(state), assetManager(assetManager)
{
}

void GridlineRenderer::populate(DrawCallMap& drawCallMap, const TimeBasedGridlineMap& gridlines,
                                double windowStartTime, double windowEndTime,
                                uint width, uint height,
                                float posEnd,
                                float gridlinePosOffset, float gridZOffset,
                                float farFadeEnd, float farFadeLen, float farFadeCurve)
{
    this->width = width;
    this->height = height;
    this->posEnd = posEnd;

    bool isDrums = isDrumLike(activePart);
    const auto* config = getRenderTypeConfig(getRenderType(activePart));
    const auto& fbCoords = *config->fretboardCoords;
    auto perspParams = config->getPerspectiveParams();

    // Strike-reference geometry — same for every gridline at this resolution.
    auto strikeEdge = getColumnEdge(0.0f, fbCoords, PositionConstants::GRIDLINE_WIDTH_SCALE);
    float strikeWidth = strikeEdge.rightX - strikeEdge.leftX;
    float strikeHeight = (perspParams.barNoteHeightRatio > 0.0f)
                       ? (strikeWidth / perspParams.barNoteHeightRatio)
                       : strikeWidth;

    double windowTimeSpan = windowEndTime - windowStartTime;

    for (const auto& gridline : gridlines)
    {
        double gridlineTime = gridline.time;
        Gridline gridlineType = gridline.type;

        float normalizedPosition = (float)((gridlineTime - windowStartTime) / windowTimeSpan) + gridlinePosOffset;

        if (normalizedPosition < HIGHWAY_POS_START || normalizedPosition > farFadeEnd)
            continue;

        // In write mode the default HALF_BEAT line is replaced by the user's
        // STEP grid (which acts as the sub-beat division). MEASURE and BEAT
        // stay visible — they're the rhythm structure the user is placing
        // notes within, same as any MIDI editor grid.
        if (writeMode && gridlineType == Gridline::HALF_BEAT)
            continue;

        juce::Image* markerImage = assetManager.getGridlineImage(gridlineType);
        if (markerImage == nullptr)
            continue;

        float baseOpacity = 1.0f;
        if (writeMode)
        {
            switch (gridlineType) {
                case Gridline::MEASURE:    baseOpacity = WRITE_MEASURE_OPACITY;   break;
                case Gridline::BEAT:       baseOpacity = WRITE_BEAT_OPACITY;      break;
                case Gridline::HALF_BEAT:  baseOpacity = WRITE_HALF_BEAT_OPACITY; break;
                case Gridline::STEP:       baseOpacity = WRITE_STEP_OPACITY;      break;
            }
        }
        else
        {
            switch (gridlineType) {
                case Gridline::MEASURE:    baseOpacity = MEASURE_OPACITY;   break;
                case Gridline::BEAT:       baseOpacity = BEAT_OPACITY;      break;
                case Gridline::HALF_BEAT:  baseOpacity = HALF_BEAT_OPACITY; break;
                case Gridline::STEP:       baseOpacity = HALF_BEAT_OPACITY; break; // unreachable: not emitted when writeMode off
            }
        }
        float fadeOpacity = PositionMath::bemaniMode
                          ? 1.0f
                          : calculateFarFade(normalizedPosition, farFadeEnd, farFadeLen, farFadeCurve);
        float opacity = baseOpacity * fadeOpacity;

        if (PositionMath::bemaniMode)
        {
            // Bemani gridline = horizontal gradient line, no marker image. Push
            // the draw call directly; doesn't fit the Frame model.
            float bemaniOpacity = std::min(1.0f, opacity * bemaniConfig.gridlineBoost);
            uint w = width;
            uint h = height;
            float pe = posEnd;
            Part part = activePart;
            float pos = normalizedPosition;
            drawCallMap[(int)DrawOrder::GRID][0].push_back([w, h, pe, part, pos, bemaniOpacity](juce::Graphics& g) {
                bool drums = isDrumLike(part);
                auto edge = PositionMath::getFretboardEdge(drums, pos, w, h,
                                PositionConstants::HIGHWAY_POS_START, pe);
                float lineH = std::max(1.0f, (float)w * 0.003f);
                float lineY = edge.centerY - lineH * 0.5f + bemaniConfig.gridlineZ;

                juce::ColourGradient grad(
                    juce::Colours::white.withAlpha(bemaniOpacity), (edge.leftX + edge.rightX) * 0.5f, lineY,
                    juce::Colours::white.withAlpha(bemaniOpacity * 0.3f), edge.leftX, lineY, false);
                grad.addColour(0.0, juce::Colours::white.withAlpha(bemaniOpacity * 0.3f));
                grad.addColour(0.5, juce::Colours::white.withAlpha(bemaniOpacity));
                grad.addColour(1.0, juce::Colours::white.withAlpha(bemaniOpacity * 0.3f));
                g.setGradientFill(grad);
                g.fillRect(edge.leftX, lineY, edge.rightX - edge.leftX, lineH);
            });
            continue;
        }

        // Perspective path: single-sprite Frame at the gridline's depth.
        auto edge = getColumnEdge(normalizedPosition, fbCoords, PositionConstants::GRIDLINE_WIDTH_SCALE);
        float curWidth = edge.rightX - edge.leftX;
        float widthRatio = (strikeWidth > 0.0f) ? (curWidth / strikeWidth) : 1.0f;

        juce::Point<float> anchor((edge.leftX + edge.rightX) * 0.5f, edge.centerY);
        juce::Point<float> frameScale(widthRatio, widthRatio);

        Frame frame;

        FrameSprite s;
        s.image     = markerImage;
        s.offsetX   = 0.0f;
        s.offsetY   = gridZOffset;
        s.width     = strikeWidth;
        s.height    = strikeHeight;
        s.drawOrder = (int)DrawOrder::GRID;
        s.drawColumn = 0;
        s.opacity   = opacity;
        frame.sprites.push_back(s);

        drawFrame(frame, anchor, frameScale, drawCallMap);
    }
}
