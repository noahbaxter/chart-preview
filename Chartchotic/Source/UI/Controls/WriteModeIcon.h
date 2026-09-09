#pragma once

#include <JuceHeader.h>
#include "../Theme.h"
#include "../HelpTypes.h"
#include "../../Editor/AuthoringTypes.h"

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
    WriteModeIcon()
    {
        setInterceptsMouseClicks(true, false);
        cursorIcon = juce::Drawable::createFromImageData(
            BinaryData::icon_cursor_svg, BinaryData::icon_cursor_svgSize);
    }

    void setState(bool writeModeActive, SubMode subMode)
    {
        if (active == writeModeActive && mode == subMode) return;
        active = writeModeActive;
        mode = subMode;
        repaint();
    }

    std::function<void()> onClick;
    std::function<void(const HelpText&)> onHoverHelp;
    std::function<void()> onHoverHelpClear;

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
            ? juce::Colour(Theme::purple)
            : juce::Colour(Theme::blue);
        const juce::Colour glyphColour = active
            ? (hovering ? onColour.brighter(0.12f) : onColour)
            : juce::Colour(Theme::textDim).withAlpha(hovering ? 0.75f : 0.55f);

        if (mode == SubMode::Draw)
        {
            g.setColour(glyphColour);
            drawPencil(g, box);
        }
        else if (cursorIcon)
        {
            auto shrunk = box.reduced(box.getWidth() * 0.12f);
            auto tinted = cursorIcon->createCopy();
            tinted->replaceColour(juce::Colours::black, glyphColour);
            tinted->drawWithin(g, shrunk, juce::RectanglePlacement::centred, 1.0f);
        }
    }

    void mouseEnter(const juce::MouseEvent&) override
    {
        repaint();
        if (onHoverHelp)
        {
            onHoverHelp(makeHelp({
                key("W"), dim(active ? "exit authoring mode" : "enter authoring mode")
            }));
        }
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        repaint();
        if (onHoverHelpClear) onHoverHelpClear();
    }

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

    bool    active = false;
    SubMode mode   = SubMode::Draw;
    std::unique_ptr<juce::Drawable> cursorIcon;
};
