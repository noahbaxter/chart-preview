#pragma once

#include <JuceHeader.h>
#include "../Theme.h"
#include "../../Editor/WriteController.h"

// Read-only display strip that appears below the main toolbar while write
// mode is active. Shows step division, snap state, and tuplet — all sourced
// live from WriteController. Pure display: keys ([/]/S/T) are still the only
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

        const auto step    = juce::String("1/") + juce::String(writeController.stepDivision());
        const auto snap    = juce::String("Snap: ") + (writeController.snapEnabled() ? "ON" : "OFF");
        const int  tuplet  = writeController.tuplet();
        const auto tuplStr = (tuplet == 0)
            ? juce::String("Tuplet: off")
            : (juce::String("Tuplet: ") + juce::String(tuplet));

        g.setFont(Theme::smallFont);

        // Three slots, each a fixed-ish chunk laid out horizontally with a
        // small left margin. Reading a chart, the user wants to glance at
        // these; centering the row keeps it balanced regardless of width.
        const int gap    = juce::jmax(8,  h / 3);
        const int margin = juce::jmax(8,  h / 2);

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

        // Measure each slot's natural width.
        int widths[3];
        int totalW = 0;
        for (int i = 0; i < 3; ++i)
        {
            widths[i] = juce::jmax(40, g.getCurrentFont().getStringWidth(slots[i].text) + margin);
            totalW += widths[i];
        }
        totalW += gap * 2;

        int x = juce::jmax(margin, (getWidth() - totalW) / 2);
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
