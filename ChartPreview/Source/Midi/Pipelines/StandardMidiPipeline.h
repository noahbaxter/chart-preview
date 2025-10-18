/*
  ==============================================================================

    StandardMidiPipeline.h
    Standard VST MIDI processing pipeline for non-REAPER DAWs

  ==============================================================================
*/

#pragma once

#include "MidiPipeline.h"
#include "../Processing/MidiProcessor.h"

class StandardMidiPipeline : public MidiPipeline
{
public:
    StandardMidiPipeline(MidiProcessor& processor,
                        juce::ValueTree& pluginState);

    void process(const juce::AudioPlayHead::PositionInfo& position,
                uint blockSize,
                double sampleRate) override;

    bool needsRealtimeMidiBuffer() const override;

    void setDisplayWindow(PPQ start, PPQ end) override;

    void processMidiBuffer(juce::MidiBuffer& midiMessages,
                          const juce::AudioPlayHead::PositionInfo& position,
                          uint blockSize,
                          uint latencySamples,
                          double sampleRate) override;

    PPQ getCurrentPosition() const override;
    bool isPlaying() const override;

private:
    MidiProcessor& midiProcessor;
    juce::ValueTree& state;

    PPQ currentPosition{0.0};
    bool playing = false;
};