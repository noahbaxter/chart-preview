/*
  ==============================================================================

    StandardMidiPipeline.cpp
    Standard VST MIDI processing pipeline for non-REAPER DAWs

  ==============================================================================
*/

#include "StandardMidiPipeline.h"

StandardMidiPipeline::StandardMidiPipeline(MidiProcessor& processor,
                                         juce::ValueTree& pluginState)
    : midiProcessor(processor),
      state(pluginState)
{
}

void StandardMidiPipeline::process(const juce::AudioPlayHead::PositionInfo& position,
                                   uint blockSize,
                                   double sampleRate)
{
    // Store current position for GUI
    currentPosition = position.getPpqPosition().orFallback(0.0);
    playing = position.getIsPlaying();
}

bool StandardMidiPipeline::needsRealtimeMidiBuffer() const
{
    return true; // Standard pipeline needs realtime MIDI from DAW
}

void StandardMidiPipeline::setDisplayWindow(PPQ start, PPQ end)
{
    // Standard pipeline doesn't need to prefetch data
    // The display window is handled by the MidiProcessor itself
}

void StandardMidiPipeline::processMidiBuffer(juce::MidiBuffer& midiMessages,
                                            const juce::AudioPlayHead::PositionInfo& position,
                                            uint blockSize,
                                            uint latencySamples,
                                            double sampleRate)
{
    // This is the existing MIDI processing logic from processBlock
    if (position.getIsPlaying())
    {
        midiProcessor.process(midiMessages,
                            position,
                            blockSize,
                            latencySamples,
                            sampleRate);
    }
}

PPQ StandardMidiPipeline::getCurrentPosition() const
{
    return currentPosition;
}

bool StandardMidiPipeline::isPlaying() const
{
    return playing;
}