#pragma once

#include <JuceHeader.h>
#include "../Theme.h"
#include "../../Editor/InteractionController.h"

// Read-only display strip that appears below the main toolbar while write
// mode is active. Shows snap / division / tuplet readouts grouped as a single
// cluster, all sourced live from InteractionController. The strip washes the active
// mode colour (green / coral) so the mode is visible without a separate pill.
// Pure display: keys (W/Q/[/]/S/T) are still the only way to change these.
// Clickable controls land in a later milestone.
class WriteSubToolbar : public juce::Component
{
public:
    explicit WriteSubToolbar(InteractionController& wc) : interactionController(wc)
    {
        setInterceptsMouseClicks(false, false);
    }

    // Re-read state and repaint. Called from InteractionController::onStateChanged.
    void refreshFromController() { repaint(); }

    void paint(juce::Graphics& g) override
    {
        const bool drawMode = interactionController.subMode() == SubMode::Draw;
        const juce::Colour modeColour = drawMode
            ? juce::Colour(Theme::green)
            : juce::Colour(Theme::coral);
        const bool snapOn = interactionController.snapEnabled();

        // Whole strip washes the active mode color so you can't miss which
        // mode you're in. Low alpha so the slot text stays legible.
        g.setColour(juce::Colour(Theme::darkBgLighter).withAlpha(0.85f));
        g.fillRect(getLocalBounds());
        g.setColour(modeColour.withAlpha(0.18f));
        g.fillRect(getLocalBounds());

        // Top divider in the mode color so the seam against the main toolbar
        // also reads the active mode at a glance.
        g.setColour(modeColour.withAlpha(0.55f));
        g.drawHorizontalLine(0, 0.0f, (float)getWidth());

        const int h = getHeight();
        if (h <= 0) return;

        const int margin = juce::jmax(8, h / 2);
        const int gap    = juce::jmax(6, h / 4);

        // ---- Cluster: snap pill | division | tuplet ----
        // Snap is the master gate (leftmost). When snap is OFF, division and
        // tuplet fade to textDim — they're meaningless without it.

        const juce::String divText  = juce::String("1/") + juce::String(interactionController.stepDivision());
        const int          tuplet   = interactionController.tuplet();
        const juce::String tuplText = (tuplet == 0)
            ? juce::String("Tuplet: off")
            : (juce::String("Tuplet: ") + juce::String(tuplet));

        // Snap pill geometry — match PillToggle aesthetic (small, rounded).
        const juce::String snapLabel = "Snap";
        g.setFont(Theme::smallFont);
        const int snapTextW = g.getCurrentFont().getStringWidth(snapLabel);
        const int snapPadX  = juce::jmax(8, h / 3);
        const int snapW     = snapTextW + snapPadX * 2;
        const int snapH     = juce::jmax(1, h - juce::jmax(6, h / 3));

        // Measure division + tuplet text at the same font.
        const int divW  = juce::jmax(28, g.getCurrentFont().getStringWidth(divText)  + margin / 2);
        const int tuplW = juce::jmax(60, g.getCurrentFont().getStringWidth(tuplText) + margin / 2);

        // Cluster width = snap pill + gap + division + gap + tuplet, plus
        // a little internal padding inside the cluster background.
        const int clusterPadX = juce::jmax(6, h / 4);
        const int innerW      = snapW + gap + divW + gap + tuplW;
        const int clusterW    = innerW + clusterPadX * 2;
        const int clusterH    = juce::jmax(1, h - juce::jmax(2, h / 6));
        const int clusterY    = (h - clusterH) / 2;

        // Centre the cluster horizontally with a margin guard on each edge.
        const int availW = juce::jmax(0, getWidth() - margin * 2);
        const int clusterX = juce::jmax(margin, margin + (availW - clusterW) / 2);

        // Cluster background — subtle so the wash isn't fighting the cluster
        // border. Low-alpha rounded rect spanning the trio.
        const auto clusterBounds = juce::Rectangle<float>(
            (float)clusterX, (float)clusterY, (float)clusterW, (float)clusterH);
        const float clusterCorner = juce::jmax(3.0f, (float)clusterH * 0.3f);
        g.setColour(juce::Colour(Theme::darkBgLighter).withAlpha(0.3f));
        g.fillRoundedRectangle(clusterBounds, clusterCorner);

        // ---- Snap pill (display-only; mirrors PillToggle aesthetic) ----
        const int snapX = clusterX + clusterPadX;
        const int snapY = (h - snapH) / 2;
        auto snapBounds = juce::Rectangle<float>(
            (float)snapX, (float)snapY, (float)snapW, (float)snapH).reduced(1.0f);
        const float snapCorner = Theme::pillCorner;

        if (snapOn)
        {
            // Filled in the active mode colour so the pill reinforces the wash.
            g.setColour(modeColour);
            g.fillRoundedRectangle(snapBounds, snapCorner);
            g.setColour(juce::Colour(Theme::textWhite));
        }
        else
        {
            // Outlined / dim when off — same as PillToggle's off state.
            g.setColour(juce::Colour(Theme::textDim).withAlpha(0.5f));
            g.drawRoundedRectangle(snapBounds, snapCorner, 1.0f);
            g.setColour(juce::Colour(Theme::textDim));
        }
        g.setFont(Theme::smallFont);
        g.drawText(snapLabel, juce::Rectangle<int>(snapX, snapY, snapW, snapH),
                   juce::Justification::centred);

        // ---- Division + tuplet readouts ----
        // Both fade to textDim when snap is OFF (they don't apply without snap).
        const juce::Colour divColour = snapOn
            ? juce::Colour(Theme::textWhite)
            : juce::Colour(Theme::textDim);
        const juce::Colour tuplColour = snapOn
            ? ((tuplet == 0) ? juce::Colour(Theme::textDim) : juce::Colour(Theme::yellow))
            : juce::Colour(Theme::textDim);

        int x = snapX + snapW + gap;
        g.setColour(divColour);
        g.drawText(divText, x, 0, divW, h, juce::Justification::centred);
        x += divW + gap;

        g.setColour(tuplColour);
        g.drawText(tuplText, x, 0, tuplW, h, juce::Justification::centred);
    }

private:
    InteractionController& interactionController;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WriteSubToolbar)
};
