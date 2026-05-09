#pragma once

#include <JuceHeader.h>
#include "../Theme.h"
#include "../../Editor/AuthoringTypes.h"  // for SubMode

// Floating mode-status chip rendered over the highway (top-left). Reflects
// WriteController state at a glance:
//   - Write mode OFF      : glyph drawn dim, reflects last-used sub-mode
//   - Write mode ON, Draw : glyph drawn green, pencil
//   - Write mode ON, Edit : glyph drawn coral, mouse-cursor arrow
// Clicking toggles write mode (same effect as the W key); Q still swaps
// sub-mode. State is pushed via setState() from PluginEditor (which forwards
// WriteController::onStateChanged).
class WriteModeIcon : public juce::Component
{
public:
    WriteModeIcon() { setInterceptsMouseClicks(true, false); }

    void setState(bool writeModeActive, SubMode subMode)
    {
        if (active == writeModeActive && mode == subMode) return;
        active = writeModeActive;
        mode = subMode;
        repaint();
    }

    // Fired on click — wire to WriteController toggle in PluginEditor.
    std::function<void()> onClick;

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        const bool hovering = isMouseOver();

        // Frame: subtle rounded chip so the icon reads as a distinct UI
        // element on the dark highway. Hover slightly brightens the fill +
        // edge to advertise interactivity.
        const float corner = juce::jmax(3.0f, bounds.getHeight() * 0.22f);
        const float fillAlpha   = hovering ? 0.6f : 0.4f;
        const float borderAlpha = hovering ? 0.45f : 0.25f;

        auto frame = bounds.reduced(0.5f);
        g.setColour(juce::Colour(Theme::darkBgLighter).withAlpha(fillAlpha));
        g.fillRoundedRectangle(frame, corner);
        g.setColour(juce::Colour(Theme::textDim).withAlpha(borderAlpha));
        g.drawRoundedRectangle(frame, corner, 1.0f);

        // Glyph occupies an inset square inside the frame so it never kisses
        // the rounded border.
        const float pad = juce::jmax(3.0f, bounds.getHeight() * 0.18f);
        auto inner = bounds.reduced(pad);
        const float side = juce::jmin(inner.getWidth(), inner.getHeight());
        auto box = juce::Rectangle<float>(side, side).withCentre(inner.getCentre());

        const juce::Colour onColour = (mode == SubMode::Draw)
            ? juce::Colour(Theme::green)
            : juce::Colour(Theme::coral);
        const juce::Colour glyphColour = active
            ? (hovering ? onColour.brighter(0.12f) : onColour)
            : juce::Colour(Theme::textDim).withAlpha(hovering ? 0.75f : 0.55f);

        g.setColour(glyphColour);

        if (mode == SubMode::Draw)
            drawPencil(g, box);
        else
            drawArrow(g, box);
    }

    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (getLocalBounds().contains(e.getPosition()) && onClick)
            onClick();
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
