/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Midi/MidiInterpreter.h"
#include "Renderers/HighwayRenderer.h"
#include "Utils.h"

//==============================================================================
/**
*/
class ChartPreviewAudioProcessorEditor  : 
    public juce::AudioProcessorEditor,
    private juce::ComboBox::Listener,
    private juce::Slider::Listener,
    private juce::ToggleButton::Listener,
    private juce::Timer
{
public:
    ChartPreviewAudioProcessorEditor (ChartPreviewAudioProcessor&, juce::ValueTree &state);
    ~ChartPreviewAudioProcessorEditor() override;

    //==============================================================================
    void timerCallback() override
    {
        printCallback();
        repaint();
    }

    void paint (juce::Graphics&) override;
    void resized() override;

    //==============================================================================
    // UI Callbacks

    void comboBoxChanged(juce::ComboBox *comboBoxThatHasChanged) override
    {
        if (comboBoxThatHasChanged == &skillMenu)
        {
            auto skillValue = skillMenu.getSelectedId();
            state.setProperty("skillLevel", skillValue, nullptr);
        }
        else if (comboBoxThatHasChanged == &partMenu)
        {
            auto partValue = partMenu.getSelectedId();
            state.setProperty("part", partValue, nullptr);
        }
        else if (comboBoxThatHasChanged == &drumTypeMenu)
        {
            auto drumTypeValue = drumTypeMenu.getSelectedId();
            state.setProperty("drumType", drumTypeValue, nullptr);
        }
        else if (comboBoxThatHasChanged == &framerateMenu)
        {
            auto framerateValue = framerateMenu.getSelectedId();
            state.setProperty("framerate", framerateValue, nullptr);
            int frameRate;
            switch (framerateValue)
            {
            case 1:
                frameRate = 15;
                break;
            case 2:
                frameRate = 30;
                break;
            case 3:
                frameRate = 60;
                break;
            }
            startTimerHz(frameRate);
        }
        else if (comboBoxThatHasChanged == &latencyMenu)
        {
            auto latencyValue = latencyMenu.getSelectedId();
            state.setProperty("latency", latencyValue, nullptr);

            switch (latencyValue)
            {
            case 1:
                latencyInSeconds = 0.250;
                break;
            case 2:
                latencyInSeconds = 0.500;
                break;
            case 3:
                latencyInSeconds = 0.750;
                break;
            case 4:
                latencyInSeconds = 1.000;
                break;
            }

            audioProcessor.setLatencyInSeconds(latencyInSeconds);
        }
    }

    void sliderValueChanged(juce::Slider *slider) override
    {
        if (slider == &chartZoomSlider)
        {
            
            displaySizeInSeconds = slider->getValue();
            displaySizeInSamples = int(displaySizeInSeconds * audioProcessor.getSampleRate());
        }
    }

    void buttonClicked(juce::Button * button) override
    {
        if (button == &starPowerToggle)
        {
            bool buttonState = button->getToggleState();
            state.setProperty("starPower", buttonState ? 1 : 0, nullptr);
        }
        else if (button == &kick2xToggle)
        {
            bool buttonState = button->getToggleState();
            state.setProperty("kick2x", buttonState ? 1 : 0, nullptr);
        }
        else if (button == &dynamicsToggle)
        {
            bool buttonState = button->getToggleState();
            state.setProperty("dynamics", buttonState ? 1 : 0, nullptr);
        }
    }

private:
    juce::ValueTree& state;

    ChartPreviewAudioProcessor& audioProcessor;
    MidiInterpreter midiInterpreter;
    HighwayRenderer highwayRenderer;

    //==============================================================================
    // UI Elements
    int defaultWidth = 800, defaultHeight = 600;

    // Background Assets
    juce::Image backgroundImage;
    juce::Image trackDrumImage;
    juce::Image trackGuitarImage;

    juce::Label chartZoomLabel;
    juce::ComboBox skillMenu, partMenu, drumTypeMenu, framerateMenu, latencyMenu;
    juce::ToggleButton starPowerToggle, kick2xToggle, dynamicsToggle;
    juce::Slider chartZoomSlider;

    juce::TextEditor consoleOutput;
    juce::ToggleButton debugToggle;

    //==============================================================================

    void initState();
    void initAssets();
    void initMenus();

    float latencyInSeconds = 0.0;

    float displaySizeInSeconds = 0.5;
    int displaySizeInSamples = int(displaySizeInSeconds * audioProcessor.getSampleRate());

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChartPreviewAudioProcessorEditor)

    // Position helpers

    int currentPlayheadPositionInSamples()
    {
        return audioProcessor.playheadPositionInSamples;
    }

    double currentPlayheadPositionInPPQ()
    {
        return audioProcessor.playheadPositionInPPQ;
    }

    double defaultLatencyInPPQ = 0.0;
    double defaultBPM = 120.0;
    double defaultDisplaySizeInPPQ = 4.0; // 4 beats

    double latencyInPPQ()
    {
        if (audioProcessor.getPlayHead() == nullptr) return defaultLatencyInPPQ;

        auto positionInfo = audioProcessor.getPlayHead()->getPosition();
        if (!positionInfo.hasValue()) return defaultLatencyInPPQ;

        double bpm = positionInfo->getBpm().orFallback(defaultBPM);
        return audioProcessor.latencyInSeconds * (bpm / 60.0);
    }

    double displaySizeInPPQ()
    {
        if (audioProcessor.getPlayHead() == nullptr) return defaultDisplaySizeInPPQ;
        
        auto positionInfo = audioProcessor.getPlayHead()->getPosition();
        if (!positionInfo.hasValue()) return defaultDisplaySizeInPPQ;

        double bpm = positionInfo->getBpm().orFallback(defaultBPM);
        return displaySizeInSeconds * (bpm / 60.0);
    }

    //==============================================================================
    // Prints

    void print(const juce::String &line)
    {
        audioProcessor.debugText += line + "\n";
    }

    std::string gemsToString(std::array<Gem, 7> gems)
    {
        std::string str = "(";
        for (const auto &gem : gems)
        {
            str += std::to_string((int)gem) + ",";
        }
        str += ")";
        return str;
    }

    void printCallback()
    {
        consoleOutput.clear();
        // consoleOutput.moveCaretToEnd();

        consoleOutput.insertTextAtCaret(audioProcessor.debugText);
        audioProcessor.debugText.clear();
    }

    // void printMidiMessages()
    // {
    //     auto midiEventMap = audioProcessor.getMidiEventMap();
    //     for (const auto &item : midiEventMap)
    //     {
    //         int index = item.first;
    //         const std::vector<juce::MidiMessage> &messages = item.second;

    //         std::string str = std::to_string(index) + ": " + midiMessagesToString(messages);
    //         print(str);
    //     }
    // }

    std::string midiMessagesToString(const std::vector<juce::MidiMessage> &messages)
    {
        std::string str = "";
        for (const auto &message : messages)
        {
            str += message.getDescription().toStdString() + " ";
        }
        return str;
    }
};