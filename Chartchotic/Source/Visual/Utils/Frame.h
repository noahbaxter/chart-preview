/*
    ==============================================================================

        Frame.h
        Created by Claude Code
        Author: Noah Baxter

        Frame abstraction: a rhythm-game object as a collection of sprites
        sharing one perspective scale. Built in pixel space at strike
        reference; rendered by applying a single frameScale uniformly to
        every member's offset and size.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>

namespace Render
{
    /** One sprite in a frame. Offset + size are in pixels at the frame's
        strike reference scale. Multiplied by frameScale at render time. */
    enum class ClipHalf { None, Left, Right };

    struct FrameSprite
    {
        juce::Image* image = nullptr;
        float offsetX   = 0.0f;   // pixels from anchor.x at strike reference
        float offsetY   = 0.0f;   // pixels from anchor.y at strike reference
        float width     = 0.0f;   // strike-reference width in pixels
        float height    = 0.0f;   // strike-reference height in pixels
        int   drawOrder = 0;      // DrawOrder enum value (cast to int)
        int   drawColumn = 0;     // DrawCallMap column bucket (0..MAX_DRAW_COLUMNS-1)
        float opacity   = 1.0f;
        juce::Colour tint {};     // if alpha > 0, clip to image shape and fill
        ClipHalf clipHalf = ClipHalf::None;
    };

    /** A rhythm-game object: a list of sprites sharing one anchor + scale.
        The caller projects the anchor and computes frameScale from depth;
        drawFrame applies them uniformly so members can't drift apart. */
    struct Frame
    {
        std::vector<FrameSprite> sprites;
    };
}
