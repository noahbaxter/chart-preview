/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Midi/MidiInterpreter.h"
#include "Utils.h"

//==============================================================================
/**
*/
class ChartPreviewAudioProcessorEditor  : 
    public juce::AudioProcessorEditor,
    private juce::ComboBox::Listener,
    private juce::ToggleButton::Listener,
    private juce::Timer
{
public:
    ChartPreviewAudioProcessorEditor (ChartPreviewAudioProcessor&);
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
    // ValueTree

    void updateValueTreeState(const juce::String &property, int value)
    {
        state.setProperty(property, value, nullptr);
    }

    void comboBoxChanged(juce::ComboBox *comboBoxThatHasChanged) override
    {
        if (comboBoxThatHasChanged == &skillMenu)
        {
            // Update ValueTree state for skillMenu
            auto skillValue = skillMenu.getSelectedId();
            // Assuming you have a ValueTree named state and a method to update it
            updateValueTreeState("skillLevel", skillValue);
        }
        else if (comboBoxThatHasChanged == &partMenu)
        {
            // Update ValueTree state for partMenu
            auto partValue = partMenu.getSelectedId();
            updateValueTreeState("part", partValue);
        }
        else if (comboBoxThatHasChanged == &drumTypeMenu)
        {
            // Update ValueTree state for drumTypeMenu
            auto drumTypeValue = drumTypeMenu.getSelectedId();
            updateValueTreeState("drumType", drumTypeValue);
        }
    }

    void buttonClicked(juce::Button * button) override
    {
        if (button == &starPowerToggle)
        {
            bool buttonState = button->getToggleState();
            updateValueTreeState("starPower", buttonState ? 1 : 0);
        }
        else if (button == &kick2xToggle)
        {
            bool buttonState = button->getToggleState();
            updateValueTreeState("kick2x", buttonState ? 1 : 0);
        }
        else if (button == &dynamicsToggle)
        {
            bool buttonState = button->getToggleState();
            updateValueTreeState("dynamics", buttonState ? 1 : 0);
        }
    }

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    juce::ValueTree state;

    ChartPreviewAudioProcessor& audioProcessor;
    MidiInterpreter midiInterpreter;

    //==============================================================================
    // UI Elements
    int defaultWidth = 800, defaultHeight = 600;

    juce::Image backgroundImage,
                drumTrackImage, guitarTrackImage,
                halfBeatImage, measureImage,

                gemKickImage, 
                gemGreenImage, 
                gemRedImage, 
                gemYellowImage, 
                gemBlueImage, 
                gemOrangeImage,

                gemCymYellowImage, 
                gemCymBlueImage, 
                gemCymGreenImage;

    juce::ComboBox skillMenu, partMenu, drumTypeMenu;
    juce::ToggleButton starPowerToggle, kick2xToggle, dynamicsToggle;

    juce::TextEditor consoleOutput;
    juce::ToggleButton debugToggle;

    //==============================================================================

    void initState();
    void initAssets();
    void initMenus();

    int currentPlayheadPositionInSamples()
    {
        return audioProcessor.playheadPositionInSamples;
    }

    float displaySizeInSeconds = 0.5;
    int displaySizeInSamples = int(displaySizeInSeconds * audioProcessor.getSampleRate());

    // Notes
    void drawGemGroup(juce::Graphics& g, const std::vector<uint>& gems, float position);
    void drawGem(juce::Graphics &g, uint gemColumn, uint gemType, float position);

    // Beat Lines

    int maxPosition = 4;
    struct beatLine
    {
        float position;
        bool measure;
    };

    std::vector<beatLine> beatLines = {
        {0.0f, true},
        {1.f, false},
        {2.f, false},
        {3.f, false},
        {4.f, true},
        {5.f, false},
        {6.f, false},
        {7.f, false},
        {8.0f, true}};

    void drawMeasureLine(juce::Graphics& g, float x, bool measure);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChartPreviewAudioProcessorEditor)

    //==============================================================================
    // Prints

    void print(const juce::String &line)
    {
        audioProcessor.debugText += line + "\n";
    }

    void printCallback()
    {
        // consoleOutput.clear();
        // consoleOutput.moveCaretToEnd();

        consoleOutput.insertTextAtCaret(audioProcessor.debugText);
        audioProcessor.debugText.clear();
    }

    void printMidiMessages()
    {
        auto midiMap = audioProcessor.getMidiMap();
        for (const auto &item : midiMap)
        {
            int index = item.first;
            const std::vector<juce::MidiMessage> &messages = item.second;

            std::string str = std::to_string(index) + ": " + midiMessagesToString(messages);
            print(str);
        }
    }

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
