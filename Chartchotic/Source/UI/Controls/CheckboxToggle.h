#pragma once

#include <JuceHeader.h>
#include "../Theme.h"

// Checkbox-style toggle: small square box with checkmark + label text.
// Coral fill when checked, outlined when unchecked. Matches theme.
class CheckboxToggle : public juce::Component
{
public:
    CheckboxToggle(const juce::String& text) : label(text) {}
    CheckboxToggle(const juce::String& text, const juce::String& hover) : label(text), hoverLabel(hover) {}

    void setToggleState(bool shouldBeOn, juce::NotificationType notification = juce::dontSendNotification)
    {
        const bool wasMixed = mixed;
        mixed = false;
        if (on == shouldBeOn && !wasMixed) return;
        on = shouldBeOn;
        repaint();
        if (notification != juce::dontSendNotification && onClick)
            onClick();
    }

    bool getToggleState() const { return on; }

    /** Selected items disagree: show a dash instead of on or off. */
    void setMixed(bool isMixed)
    {
        if (mixed == isMixed) return;
        mixed = isMixed;
        repaint();
    }

    bool isMixed() const { return mixed; }

    std::function<void()> onClick;

    void paint(juce::Graphics& g) override
    {
        float h = (float)getHeight();
        float boxSize = h * 0.7f;
        float boxY = (h - boxSize) * 0.5f;
        float boxX = 2.0f;
        auto box = juce::Rectangle<float>(boxX, boxY, boxSize, boxSize);
        float corner = boxSize * 0.2f;
        bool hovering = isMouseOver() && isEnabled();

        // Per colour: setOpacity is undone by the next setColour.
        const float dim = isEnabled() ? 1.0f : 0.35f;

        if (mixed)
        {
            g.setColour((hovering ? juce::Colour(Theme::coral).withAlpha(0.7f)
                                  : juce::Colour(Theme::textDim).withAlpha(0.5f))
                            .withMultipliedAlpha(dim));
            g.drawRoundedRectangle(box, corner, 1.0f);

            g.setColour(juce::Colour(Theme::textDim).withMultipliedAlpha(dim));
            const float inset = boxSize * 0.26f;
            g.fillRect(box.getX() + inset, box.getCentreY() - boxSize * 0.06f,
                       boxSize - inset * 2.0f, boxSize * 0.12f);
        }
        else if (on)
        {
            g.setColour((hovering ? juce::Colour(Theme::coral).brighter(0.15f)
                                  : juce::Colour(Theme::coral)).withMultipliedAlpha(dim));
            g.fillRoundedRectangle(box, corner);

            // Checkmark
            g.setColour(juce::Colour(Theme::textWhite).withMultipliedAlpha(dim));
            float cx = box.getCentreX();
            float cy = box.getCentreY();
            float s = boxSize * 0.25f;
            juce::Path check;
            check.startNewSubPath(cx - s, cy);
            check.lineTo(cx - s * 0.3f, cy + s * 0.7f);
            check.lineTo(cx + s, cy - s * 0.6f);
            g.strokePath(check, juce::PathStrokeType(boxSize * 0.12f,
                juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
        }
        else
        {
            g.setColour((hovering ? juce::Colour(Theme::coral).withAlpha(0.7f)
                                  : juce::Colour(Theme::textDim).withAlpha(0.5f))
                            .withMultipliedAlpha(dim));
            g.drawRoundedRectangle(box, corner, 1.0f);
        }

        // Label
        float labelX = boxX + boxSize + boxSize * 0.4f;
        g.setColour((hovering ? juce::Colour(Theme::textWhite)
                              : juce::Colour(Theme::textDim)).withMultipliedAlpha(dim));
        g.setFont(Theme::controlFont);
        auto& displayLabel = (hovering && hoverLabel.isNotEmpty()) ? hoverLabel : label;
        g.drawText(displayLabel, juce::Rectangle<float>(labelX, 0.0f, getWidth() - labelX, h),
                   juce::Justification::centredLeft);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (!isEnabled()) return;
        if (getLocalBounds().contains(e.getPosition()))
        {
            // Mixed resolves upwards: one click turns the whole selection on.
            on = mixed ? true : !on;
            mixed = false;
            repaint();
            if (onClick) onClick();
        }
    }

    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

private:
    juce::String label;
    juce::String hoverLabel;
    bool on = false;
    bool mixed = false;
};
