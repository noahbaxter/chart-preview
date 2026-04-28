#pragma once

#include <JuceHeader.h>
#include "../Theme.h"
#include "../../Editor/WriteController.h"  // for SubMode

// Compact pill that reflects WriteController state:
//   - Write mode OFF  : muted "EDIT" label (toolbar looks pre-M1)
//   - Write mode ON, Draw : filled green "DRAW"
//   - Write mode ON, Edit : filled coral "EDIT"
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
        auto cornerSize = Theme::pillCorner;

        juce::String label;
        juce::Colour fill;
        juce::Colour text;

        if (!active)
        {
            // Muted outline — matches PillToggle "off" appearance.
            label = "EDIT";
            g.setColour(juce::Colour(Theme::textDim).withAlpha(0.5f));
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
            text = juce::Colour(Theme::textDim);
        }
        else if (mode == SubMode::Draw)
        {
            label = "DRAW";
            fill = juce::Colour(Theme::green);
            g.setColour(fill);
            g.fillRoundedRectangle(bounds, cornerSize);
            text = juce::Colour(Theme::textWhite);
        }
        else
        {
            label = "EDIT";
            fill = juce::Colour(Theme::coral);
            g.setColour(fill);
            g.fillRoundedRectangle(bounds, cornerSize);
            text = juce::Colour(Theme::textWhite);
        }

        g.setColour(text);
        g.setFont(Theme::smallFont);
        g.drawText(label, getLocalBounds(), juce::Justification::centred);
    }

private:
    bool    active = false;
    SubMode mode   = SubMode::Draw;
};
