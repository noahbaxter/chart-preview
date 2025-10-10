/*
  ==============================================================================

    MidiPipelineFactory.h
    Factory for creating the appropriate MIDI processing pipeline

  ==============================================================================
*/

#pragma once

#include <memory>
#include "MidiPipeline.h"

class MidiProcessor;
class ReaperMidiProvider;

class MidiPipelineFactory
{
public:
    // Creates the appropriate pipeline based on the host and configuration
    static std::unique_ptr<MidiPipeline> createPipeline(
        bool isReaperHost,
        bool useReaperTimeline,
        MidiProcessor& midiProcessor,
        ReaperMidiProvider* reaperProvider,
        juce::ValueTree& state,
        std::function<void(const juce::String&)> printFunc = nullptr
    );
};