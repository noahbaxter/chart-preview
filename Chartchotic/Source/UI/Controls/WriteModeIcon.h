#pragma once

#include <JuceHeader.h>
#include "../Theme.h"
#include "../../Editor/WriteController.h"  // for SubMode

// Tiny non-interactive icon shown at the top-left of the main toolbar that
// reflects WriteController state at a glance:
//   - Write mode OFF      : glyph drawn dim, reflects last-used sub-mode
//   - Write mode ON, Draw : glyph drawn green, pencil
//   - Write mode ON, Edit : glyph drawn coral, mouse-cursor arrow
// Pure display — interaction stays on W (toggle) and Q (swap sub-mode).
// State is pushed via setState() from ToolbarComponent::refreshFromWriteController().
class WriteModeIcon : public juce::Component
{
public:
    WriteModeIcon() { setInterceptsMouseClicks(false, false); }

    void setState(bool writeModeActive, SubMode subMode)
    {
        if (active == writeModeActive && mode == subMode) return;
        active = writeModeActive;
        mode = subMode;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        // Use the smaller of width/height as the design unit so the glyph stays
        // square and centered no matter the bounding box.
        const float side = juce::jmin(bounds.getWidth(), bounds.getHeight());
        auto box = juce::Rectangle<float>(side, side).withCentre(bounds.getCentre());

        const juce::Colour onColour = (mode == SubMode::Draw)
            ? juce::Colour(Theme::green)
            : juce::Colour(Theme::coral);
        const juce::Colour colour = active
            ? onColour
            : juce::Colour(Theme::textDim).withAlpha(0.55f);

        g.setColour(colour);

        if (mode == SubMode::Draw)
            drawPencil(g, box);
        else
            drawArrow(g, box);
    }

private:
    // Pencil glyph: a tilted body with a pointed tip on the bottom-right and a
    // small flat eraser stub on the top-left. Drawn as a filled Path so it
    // stays crisp at any scale.
    static void drawPencil(juce::Graphics& g, juce::Rectangle<float> box)
    {
        const float s = box.getWidth();
        const float cx = box.getCentreX();
        const float cy = box.getCentreY();

        // Build the pencil along a diagonal from top-left to bottom-right, then
        // rotate around the centre. Coordinates are relative to (cx, cy).
        const float halfLen = s * 0.42f;     // body length / 2
        const float halfThick = s * 0.13f;   // body thickness / 2
        const float tipLen = s * 0.16f;      // length of the pointed tip
        const float eraserLen = s * 0.08f;   // length of the eraser stub

        // Untransformed (horizontal) pencil pointing to the right.
        // Order: top-left of body → top-right of body → tip → bottom-right of
        // body → bottom-left of body → close.
        const float bodyL = -halfLen;
        const float bodyR = halfLen - tipLen;
        const float tipX  = halfLen;
        const float eraserL = bodyL - eraserLen;

        juce::Path p;
        // Eraser stub (small rectangle at the back)
        p.startNewSubPath(eraserL, -halfThick);
        p.lineTo(bodyL,    -halfThick);
        p.lineTo(bodyL,     halfThick);
        p.lineTo(eraserL,   halfThick);
        p.closeSubPath();

        // Body + tip
        p.startNewSubPath(bodyL, -halfThick);
        p.lineTo(bodyR,    -halfThick);
        p.lineTo(tipX,      0.0f);
        p.lineTo(bodyR,     halfThick);
        p.lineTo(bodyL,     halfThick);
        p.closeSubPath();

        // Rotate -45 degrees so the tip points to the bottom-right (classic
        // pencil orientation), then translate to the icon centre.
        auto t = juce::AffineTransform::rotation(-juce::MathConstants<float>::pi * 0.25f)
                    .translated(cx, cy);
        p.applyTransform(t);

        g.fillPath(p);
    }

    // Mouse cursor / arrow glyph: classic OS pointer outline. A triangle that
    // points up-and-to-the-left, plus a small tail on the bottom-right. Drawn
    // as a single closed Path.
    static void drawArrow(juce::Graphics& g, juce::Rectangle<float> box)
    {
        const float s = box.getWidth();
        // Inset a little so the glyph doesn't kiss the bounding box.
        const float pad = s * 0.12f;
        const float left = box.getX() + pad;
        const float top = box.getY() + pad;
        const float size = s - pad * 2.0f;

        // Standard cursor shape, drawn as if inside a unit box from the
        // top-left tip outward. Coordinates are normalised then scaled.
        // Points (in unit space):
        //   tip:        (0.00, 0.00)
        //   left edge:  (0.00, 0.78)  — straight down the left side
        //   notch in:   (0.30, 0.62)  — concave step inward
        //   tail base1: (0.42, 0.92)
        //   tail base2: (0.58, 0.86)
        //   notch out:  (0.46, 0.56)
        //   right edge: (0.78, 0.46)  — diagonal back up to near-tip
        struct P { float x, y; };
        const P pts[] = {
            { 0.00f, 0.00f },
            { 0.00f, 0.78f },
            { 0.30f, 0.62f },
            { 0.42f, 0.92f },
            { 0.58f, 0.86f },
            { 0.46f, 0.56f },
            { 0.78f, 0.46f },
        };

        juce::Path p;
        p.startNewSubPath(left + pts[0].x * size, top + pts[0].y * size);
        for (int i = 1; i < (int)(sizeof(pts) / sizeof(pts[0])); ++i)
            p.lineTo(left + pts[i].x * size, top + pts[i].y * size);
        p.closeSubPath();

        g.fillPath(p);
    }

    bool    active = false;
    SubMode mode   = SubMode::Draw;
};
