/*
    ==============================================================================

        GridlineRenderer.cpp
        Author:  Noah Baxter

        Gridline rendering extracted from SceneRenderer.

    ==============================================================================
*/

#include "GridlineRenderer.h"

using namespace PositionConstants;

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

    double windowTimeSpan = windowEndTime - windowStartTime;

    for (const auto& gridline : gridlines)
    {
        double gridlineTime = gridline.time;
        Gridline gridlineType = gridline.type;

        float normalizedPosition = (float)((gridlineTime - windowStartTime) / windowTimeSpan) + gridlinePosOffset;

        if (normalizedPosition >= HIGHWAY_POS_START && normalizedPosition <= farFadeEnd)
        {
            juce::Image* markerImage = assetManager.getGridlineImage(gridlineType);

            if (markerImage != nullptr)
            {
                float fadeOpacity = PositionMath::bemaniMode ? 1.0f : calculateFarFade(normalizedPosition, farFadeEnd, farFadeLen, farFadeCurve);

                float opacity = 1.0f;
                if (writeMode)
                {
                    switch (gridlineType) {
                        case Gridline::MEASURE:  opacity = 1.0f; break;
                        case Gridline::BEAT:     opacity = 0.6f; break;
                        case Gridline::HALF_BEAT: opacity = 0.35f; break;
                        case Gridline::STEP:     opacity = 0.25f; break;
                    }
                }
                else
                {
                    switch (gridlineType) {
                        case Gridline::MEASURE:  opacity = MEASURE_OPACITY; break;
                        case Gridline::BEAT:     opacity = BEAT_OPACITY; break;
                        case Gridline::HALF_BEAT: opacity = HALF_BEAT_OPACITY; break;
                        case Gridline::STEP:     opacity = HALF_BEAT_OPACITY * 0.7f; break;
                    }
                }
                opacity *= fadeOpacity;
                if (PositionMath::bemaniMode)
                    opacity = std::min(1.0f, opacity * bemaniConfig.gridlineBoost);

                GridlinePainter::Params gp {
                    normalizedPosition, markerImage, opacity,
                    activePart, this->width, this->height, posEnd, gridZOffset
                };

                drawCallMap[static_cast<int>(DrawOrder::GRID)][0].push_back([gp](juce::Graphics& g) {
                    if (PositionMath::bemaniMode)
                        GridlinePainter::paintBemani(g, gp);
                    else
                        GridlinePainter::paintPerspective(g, gp);
                });
            }
        }
    }
}
