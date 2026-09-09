/*
    ==============================================================================

        FrameRenderer.cpp
        Created by Claude Code
        Author: Noah Baxter

    ==============================================================================
*/

#include "FrameRenderer.h"

namespace Render
{
    void drawFrame(const Frame& frame,
                   juce::Point<float> anchor,
                   juce::Point<float> scale,
                   DrawCallMap& drawCalls)
    {
        for (const auto& sprite : frame.sprites)
        {
            if (sprite.image == nullptr) continue;

            float cx = anchor.x + sprite.offsetX * scale.x;
            float cy = anchor.y + sprite.offsetY * scale.y;
            float w  = sprite.width  * scale.x;
            float h  = sprite.height * scale.y;

            juce::Rectangle<float> rect(cx - w * 0.5f, cy - h * 0.5f, w, h);

            int order = sprite.drawOrder;
            int col   = juce::jlimit(0, MAX_DRAW_COLUMNS - 1, sprite.drawColumn);
            float opacity = sprite.opacity;
            const juce::Image* img = sprite.image;

            juce::Colour tint = sprite.tint;
            drawCalls[order][col].push_back([img, opacity, rect, tint](juce::Graphics& g) {
                g.setOpacity(opacity);
                g.drawImage(*img, rect);
                if (tint.getAlpha() > 0)
                {
                    auto transform = juce::AffineTransform::scale(
                        rect.getWidth()  / (float)img->getWidth(),
                        rect.getHeight() / (float)img->getHeight())
                        .translated(rect.getX(), rect.getY());
                    juce::Graphics::ScopedSaveState save(g);
                    g.reduceClipRegion(*img, transform);
                    g.setColour(tint);
                    g.fillAll();
                }
            });
        }
    }
}
