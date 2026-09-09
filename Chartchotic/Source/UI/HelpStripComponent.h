#pragma once

#include <JuceHeader.h>
#include "Theme.h"

class HelpStripComponent : public juce::Component
{
public:
    static constexpr float heightRatio = 0.025f;
    static constexpr int maxHeight = 22;

    HelpStripComponent()
    {
        helpLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.45f));
        helpLabel.setJustificationType(juce::Justification::centred);
        helpLabel.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(helpLabel);
    }

    void setHelpText(const juce::String& text)
    {
        helpOverride = text;
        helpLabel.setText(text, juce::dontSendNotification);
    }

    void clearHelpText()
    {
        helpOverride = {};
        helpLabel.setText(helpDefault, juce::dontSendNotification);
    }

    void setDefaultHelpText(const juce::String& text)
    {
        helpDefault = text;
        if (helpOverride.isEmpty())
            helpLabel.setText(helpDefault, juce::dontSendNotification);
    }

    void resized() override
    {
        helpLabel.setFont(Theme::getUIFont(juce::jmax(10.0f, getHeight() * 0.65f)));
        helpLabel.setBounds(getLocalBounds());
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(Theme::darkBgLighter));
        g.fillRect(getLocalBounds());
    }

private:
    juce::Label helpLabel;
    juce::String helpDefault;
    juce::String helpOverride;
};
