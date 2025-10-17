/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <memory>
#include "Midi/MidiProcessor.h"
#include "Midi/ReaperMidiProvider.h"
#include "DebugTools/Logger.h"

// Forward declarations
class MidiPipeline;

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
    juce::CriticalSection& getNoteStateMapLock() { return midiProcessor.noteStateMapLock; }
    
    // Set visual window bounds for conservative cleanup during tempo changes
    void setMidiProcessorVisualWindowBounds(PPQ startPPQ, PPQ endPPQ) { midiProcessor.setVisualWindowBounds(startPPQ, endPPQ); }
    void refreshMidiDisplay() { midiProcessor.refreshMidiDisplay(); }
    void invalidateReaperCache();  // Clear cache and force re-fetch (for track changes)

    // Debug
    juce::String debugText;
    void print(const juce::String &line) { debugText += line + "\n"; }
    void clearDebugText() { debugText.clear(); }

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

    // Process REAPER timeline MIDI for a specific window (called from editor)
    void processReaperTimelineMidi(PPQ startPPQ, PPQ endPPQ, double bpm, uint timeSignatureNumerator, uint timeSignatureDenominator);

    // Get the current MIDI pipeline
    MidiPipeline* getMidiPipeline() { return midiPipeline.get(); }

    // Set display window size (called from editor)
    void setDisplayWindowSize(PPQ size) { displayWindowSize = size; }
    PPQ getDisplayWindowSize() const { return displayWindowSize; }

    // Public state access for pipelines
    juce::ValueTree& getState() { return state; }

    // Public midiProcessor access for pipelines
    MidiProcessor& getMidiProcessor() { return midiProcessor; }

    // Debug logger access
    DebugTools::Logger& getDebugLogger() { return debugLogger; }

    // Timeline position control for scroll wheel seeking
    void requestTimelinePositionChange(PPQ newPosition);

  private:
    juce::ValueTree state;
    MidiProcessor midiProcessor;
    DebugTools::Logger debugLogger;

    // MIDI processing pipeline (created based on host)
    std::unique_ptr<MidiPipeline> midiPipeline;

    // Display window size (set by editor, used by pipeline)
    PPQ displayWindowSize = PPQ(4.0);

    // Track REAPER connection state per-instance (not static!)
    bool lastReaperConnected = false;

    void initializeDefaultState();

    // VST2 extensions instance (forward declared in .cpp)
    std::unique_ptr<juce::VST2ClientExtensions> vst2Extensions;

    // VST3 extensions instance (forward declared in .cpp)
    std::unique_ptr<juce::VST3ClientExtensions> vst3Extensions;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChartPreviewAudioProcessor)
};
