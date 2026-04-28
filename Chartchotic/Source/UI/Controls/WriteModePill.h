#pragma once

#include <JuceHeader.h>
#include "../Theme.h"
#include "../../Editor/WriteController.h"  // for SubMode

// Split pill that reflects WriteController state. Always shows both halves
// (DRAW | EDIT) so the user can see the available sub-modes at a glance.
//   - Write mode OFF      : both halves outlined, no fill
//   - Write mode ON, Draw : DRAW half filled green, EDIT half outlined
//   - Write mode ON, Edit : EDIT half filled coral, DRAW half outlined
// Pure display — no interaction. State is pushed via setState().
class WriteModePill : public juce::Component
{
public:
    WriteModePill() { setInterceptsMouseClicks(false, false); }

    void setState(bool writeModeActive, SubMode subMode)
    {
        if (active == writeModeActive && mode == subMode) return;
        active = writeModeActive;
        mode = subMode;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        const float cornerSize = Theme::pillCorner;

        // Split into halves at the midpoint.
        const float midX = bounds.getCentreX();
        auto leftHalf  = bounds.withRight(midX);
        auto rightHalf = bounds.withLeft(midX);

        const bool drawActive = active && mode == SubMode::Draw;
        const bool editActive = active && mode == SubMode::Edit;

        // Fill the active half by clipping to it and filling the full rounded
        // rect — this leaves the outer corners rounded and the inner edge straight.
        if (drawActive)
        {
            juce::Graphics::ScopedSaveState save(g);
            g.reduceClipRegion(leftHalf.toNearestInt());
            g.setColour(juce::Colour(Theme::green));
            g.fillRoundedRectangle(bounds, cornerSize);
        }
        else if (editActive)
        {
            juce::Graphics::ScopedSaveState save(g);
            g.reduceClipRegion(rightHalf.toNearestInt());
            g.setColour(juce::Colour(Theme::coral));
            g.fillRoundedRectangle(bounds, cornerSize);
        }

        // Outline the full pill on top of any fill.
        g.setColour(juce::Colour(Theme::textDim).withAlpha(0.5f));
        g.drawRoundedRectangle(bounds, cornerSize, 1.0f);

        // Vertical divider between halves.
        g.setColour(juce::Colour(Theme::textDim).withAlpha(0.5f));
        g.drawLine(midX, bounds.getY(), midX, bounds.getBottom(), 1.0f);

        // Labels — bright on the filled half, dim on the outlined half.
        g.setFont(Theme::smallFont);

        g.setColour(drawActive ? juce::Colour(Theme::textWhite)
                               : juce::Colour(Theme::textDim));
        g.drawText("DRAW", leftHalf.toNearestInt(), juce::Justification::centred);

        g.setColour(editActive ? juce::Colour(Theme::textWhite)
                               : juce::Colour(Theme::textDim));
        g.drawText("EDIT", rightHalf.toNearestInt(), juce::Justification::centred);
    }

private:
    bool    active = false;
    SubMode mode   = SubMode::Draw;
};
