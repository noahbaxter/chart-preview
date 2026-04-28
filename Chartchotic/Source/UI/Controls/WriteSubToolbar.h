#pragma once

#include <JuceHeader.h>
#include "../Theme.h"
#include "../../Editor/WriteController.h"

// Read-only display strip that appears below the main toolbar while write
// mode is active. Headlined by a prominent DRAW/EDIT pill on the left, with
// step / snap / tuplet readouts following to the right — all sourced live
// from WriteController. Pure display: keys (W/Q/[/]/S/T) are still the only
// way to change these. Clickable controls land in a later milestone.
class WriteSubToolbar : public juce::Component
{
public:
    explicit WriteSubToolbar(WriteController& wc) : writeController(wc)
    {
        setInterceptsMouseClicks(false, false);
    }

    // Re-read state and repaint. Called from WriteController::onStateChanged.
    void refreshFromController() { repaint(); }

    void paint(juce::Graphics& g) override
    {
        // Slightly tinted strip so it reads as part of the toolbar but
        // visually distinct from the main row.
        g.setColour(juce::Colour(Theme::darkBgLighter).withAlpha(0.85f));
        g.fillRect(getLocalBounds());

        // Subtle top divider against the main toolbar.
        g.setColour(juce::Colour(Theme::textDim).withAlpha(0.15f));
        g.drawHorizontalLine(0, 0.0f, (float)getWidth());

        const int h = getHeight();
        if (h <= 0) return;

        const int margin = juce::jmax(8, h / 2);
        const int gap    = juce::jmax(8, h / 3);

        // ---- Mode pill (leftmost, prominent) ----
        // Sub-toolbar is only visible while write mode is on, so this is
        // always either DRAW or EDIT — never an off state.
        const bool drawMode = writeController.subMode() == SubMode::Draw;
        const juce::Colour pillColour = drawMode
            ? juce::Colour(Theme::green)
            : juce::Colour(Theme::coral);
        const juce::String pillLabel = drawMode ? "DRAW" : "EDIT";

        // Bigger / bolder than the slot text — this is the headline.
        const float pillFontH = juce::jmax(11.0f, (float)h * 0.55f);
        juce::Font pillFont = Theme::getUIFont(pillFontH).boldened();

        const int pillTextW = pillFont.getStringWidth(pillLabel);
        const int pillPadX  = juce::jmax(10, h / 2);
        const int pillW     = pillTextW + pillPadX * 2;
        const int pillH     = juce::jmax(1, h - juce::jmax(4, h / 5));
        const int pillY     = (h - pillH) / 2;
        const int pillX     = margin;

        auto pillBounds = juce::Rectangle<float>(
            (float)pillX, (float)pillY, (float)pillW, (float)pillH);

        const float pillCorner = juce::jmax(2.0f, (float)pillH * 0.5f);
        g.setColour(pillColour);
        g.fillRoundedRectangle(pillBounds, pillCorner);

        g.setColour(juce::Colour(Theme::textWhite));
        g.setFont(pillFont);
        g.drawText(pillLabel, pillBounds.toNearestInt(), juce::Justification::centred);

        // ---- Status slots (step / snap / tuplet) ----
        const auto step    = juce::String("1/") + juce::String(writeController.stepDivision());
        const auto snap    = juce::String("Snap: ") + (writeController.snapEnabled() ? "ON" : "OFF");
        const int  tuplet  = writeController.tuplet();
        const auto tuplStr = (tuplet == 0)
            ? juce::String("Tuplet: off")
            : (juce::String("Tuplet: ") + juce::String(tuplet));

        g.setFont(Theme::smallFont);

        struct Slot { const juce::String& text; juce::Colour fg; };
        Slot slots[3] = {
            { step,    juce::Colour(Theme::textWhite) },
            { snap,    writeController.snapEnabled()
                         ? juce::Colour(Theme::green)
                         : juce::Colour(Theme::textDim) },
            { tuplStr, (tuplet == 0)
                         ? juce::Colour(Theme::textDim)
                         : juce::Colour(Theme::yellow) },
        };

        // Measure each slot's natural width (mirrors the previous layout).
        int widths[3];
        int totalW = 0;
        for (int i = 0; i < 3; ++i)
        {
            widths[i] = juce::jmax(40, g.getCurrentFont().getStringWidth(slots[i].text) + margin);
            totalW += widths[i];
        }
        totalW += gap * 2;

        // Slots flow to the right of the pill, centered in the remaining
        // space so a wide window keeps them visually balanced.
        const int slotsLeft = pillX + pillW + gap;
        int x = juce::jmax(slotsLeft, slotsLeft + (getWidth() - slotsLeft - margin - totalW) / 2);
        for (int i = 0; i < 3; ++i)
        {
            g.setColour(slots[i].fg);
            g.drawText(slots[i].text, x, 0, widths[i], h, juce::Justification::centred);
            x += widths[i] + gap;
        }
    }

private:
    WriteController& writeController;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WriteSubToolbar)
};
