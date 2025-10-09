/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Midi/MidiProcessor.h"
#include "Midi/ReaperMidiProvider.h"

//==============================================================================
/**
*/
class ChartPreviewAudioProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
public:
    ChartPreviewAudioProcessor();
    ~ChartPreviewAudioProcessor() override;

    uint playheadPositionInSamples = 0;
    PPQ playheadPositionInPPQ = 0.0;
    bool isPlaying = false;

    // Cursor position tracking for scrubbing support
    PPQ lastCursorPosition = 0.0;
    bool cursorPositionChanged = false;
    
    float latencyInSeconds = 0.5;
    uint latencyInSamples = 0;
    void setLatencyInSeconds(float latencyInSeconds);
    
    NoteStateMapArray& getNoteStateMapArray() { return midiProcessor.noteStateMapArray; }
    GridlineMap& getGridlineMap() { return midiProcessor.gridlineMap; }
    juce::CriticalSection& getGridlineMapLock() { return midiProcessor.gridlineMapLock; }
    juce::CriticalSection& getNoteStateMapLock() { return midiProcessor.noteStateMapLock; }
    
    // Set visual window bounds for conservative cleanup during tempo changes
    void setMidiProcessorVisualWindowBounds(PPQ startPPQ, PPQ endPPQ) { midiProcessor.setVisualWindowBounds(startPPQ, endPPQ); }
    void refreshMidiDisplay() { midiProcessor.refreshMidiDisplay(); }

    // Debug
    juce::String debugText;
    void print(const juce::String &line) { debugText += line + "\n"; }
    
    // Overrides
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

   // Stock JUCE
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
#endif

    // VST2 REAPER Integration
    juce::VST2ClientExtensions* getVST2ClientExtensions() override;

    // VST3 REAPER Integration
    juce::VST3ClientExtensions* getVST3ClientExtensions() override;

    // REAPER API function pointers (populated via VST2 or VST3)
    void* (*reaperGetFunc)(const char*) = nullptr;
    bool isReaperHost = false;
    bool attemptReaperConnection();
    void* getReaperApi(const char* funcname);
    std::string getHostInfo();

    // REAPER MIDI timeline access
    ReaperMidiProvider reaperMidiProvider;
    ReaperMidiProvider& getReaperMidiProvider() { return reaperMidiProvider; }

  private:
    juce::ValueTree state;
    MidiProcessor midiProcessor;

    void initializeDefaultState();

    // VST2 extensions instance (forward declared in .cpp)
    std::unique_ptr<juce::VST2ClientExtensions> vst2Extensions;

    // VST3 extensions instance (forward declared in .cpp)
    std::unique_ptr<juce::VST3ClientExtensions> vst3Extensions;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChartPreviewAudioProcessor)
};
