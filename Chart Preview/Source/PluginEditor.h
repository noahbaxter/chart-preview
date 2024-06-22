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
    private juce::Slider::Listener,
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
            auto skillValue = skillMenu.getSelectedId();
            updateValueTreeState("skillLevel", skillValue);
        }
        else if (comboBoxThatHasChanged == &partMenu)
        {
            auto partValue = partMenu.getSelectedId();
            updateValueTreeState("part", partValue);
        }
        else if (comboBoxThatHasChanged == &drumTypeMenu)
        {
            auto drumTypeValue = drumTypeMenu.getSelectedId();
            updateValueTreeState("drumType", drumTypeValue);
        }
        else if (comboBoxThatHasChanged == &framerateMenu)
        {
            auto framerateValue = framerateMenu.getSelectedId();
            updateValueTreeState("framerate", framerateValue);
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
        gemKickStyleImage,
        gemStyleImage,

        gemHOPOGreenImage,
        gemHOPORedImage,
        gemHOPOYellowImage,
        gemHOPOBlueImage,
        gemHOPOOrangeImage,
        gemHOPOStyleImage,

        gemCymYellowImage,
        gemCymBlueImage,
        gemCymGreenImage,
        gemCymStyleImage;

    juce::Label chartZoomLabel;
    juce::ComboBox skillMenu, partMenu, drumTypeMenu, framerateMenu;
    juce::ToggleButton starPowerToggle, kick2xToggle, dynamicsToggle;
    juce::Slider chartZoomSlider;

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

    float latencyInSeconds = 0.0;

    float displaySizeInSeconds = 0.5;
    int displaySizeInSamples = int(displaySizeInSeconds * audioProcessor.getSampleRate());

    bool isPart(Part part)
    {
        return (int)state.getProperty("part") == (int)part;
    }

    uint framePosition = 0;

    // Notes
    void drawFrame(juce::Graphics &g, const std::array<Gem,7> &gems, float position);
    void drawGuitarGem(juce::Graphics &g, uint gemColumn, Gem gem, float position);
    void drawDrumGem(juce::Graphics &g, uint gemColumn, Gem gem, float position);

    juce::Rectangle<float> getGuitarGlyphRect(uint gemColumn, float position);
    juce::Rectangle<float> getDrumGlyphRect(uint gemColumn, float position);
    juce::Rectangle<float> createGlyphRect(float position, float normY1, float normY2, float normX1, float normX2, float normWidth1, float normWidth2, bool isBarNote);

    juce::Image getGuitarGlyphImage(Gem gem, uint gemColumn);
    juce::Image getDrumGlyphImage(Gem gem, uint gemColumn);

    void fadeInImage(juce::Image &image, float position);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChartPreviewAudioProcessorEditor)

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
