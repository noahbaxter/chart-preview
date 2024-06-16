/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Midi/MidiInterpreter.h"

//==============================================================================
/**
*/
class ChartPreviewAudioProcessorEditor  : 
    public juce::AudioProcessorEditor,
    private juce::Timer
    // public juce::ComboBox::Listener
{
public:
    ChartPreviewAudioProcessorEditor (ChartPreviewAudioProcessor&);
    ~ChartPreviewAudioProcessorEditor() override;

    void timerCallback() override
    {
        // printMidiMessages();
        
        printCallback();
        repaint();

        // audioProcessor.getCurrentPlayheadPositionInSamples();
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

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    int i = 0;
    void printCallback()
    {
        // consoleOutput.clear();
        // consoleOutput.moveCaretToEnd();
        
        consoleOutput.insertTextAtCaret(audioProcessor.debugText);
        audioProcessor.debugText.clear();

    }

    void print(const juce::String &line)
    {
        audioProcessor.debugText += line + "\n";
    }

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    ChartPreviewAudioProcessor& audioProcessor;
    MidiInterpreter midiInterpreter;

    int defaultWidth = 800, defaultHeight = 600;
    // juce::Image getGlyph(const juce::MidiMessage& midiMessage) const;

    juce::Image backgroundImage;
    juce::Image drumTrackImage, guitarTrackImage;
    juce::Image halfBeatImage, measureImage;

    juce::Image gemKickImage, 
                gemGreenImage, 
                gemRedImage, 
                gemYellowImage, 
                gemBlueImage, 
                gemOrangeImage;

    juce::Image gemCymYellowImage, 
                gemCymBlueImage, 
                gemCymGreenImage;

    juce::ComboBox skillMenu, partMenu;
    juce::ToggleButton debugToggle;

    juce::TextEditor consoleOutput;

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
};
