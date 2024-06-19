/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Midi/MidiProcessor.h"

//==============================================================================
/**
*/
class ChartPreviewAudioProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
public:
    //==============================================================================
    ChartPreviewAudioProcessor();
    ~ChartPreviewAudioProcessor() override;

    
    juce::String debugText;
    void print(const juce::String &line)
    {
      debugText += line + "\n";
    }

    uint playheadPositionInSamples = 0;

    std::map<uint, std::vector<juce::MidiMessage>> getMidiEventMap()
    {
      return midiProcessor.midiEventMap;
    }

    std::array<std::map<uint, bool>, 128> getNoteStateMaps()
    {
      return midiProcessor.noteStateMaps;
    }

    bool isPlaying = false;
    float latencyInSeconds = 0.8;

    //==============================================================================

    

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
  //==============================================================================
  MidiProcessor midiProcessor;

  // juce::MidiBuffer lookaheadBuffer;
  // int lookaheadSamples = 88200;

  int beatsToDraw = 8;

  //==============================================================================
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChartPreviewAudioProcessor)
};
