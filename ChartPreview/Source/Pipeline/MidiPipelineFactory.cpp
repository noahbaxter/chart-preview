/*
  ==============================================================================

    MidiPipelineFactory.cpp
    Factory for creating the appropriate MIDI processing pipeline

  ==============================================================================
*/

#include "MidiPipelineFactory.h"
#include "StandardMidiPipeline.h"
#include "ReaperMidiPipeline.h"
#include "../Midi/ReaperMidiProvider.h"

std::unique_ptr<MidiPipeline> MidiPipelineFactory::createPipeline(
    bool isReaperHost,
    bool useReaperTimeline,
    MidiProcessor& midiProcessor,
    ReaperMidiProvider* reaperProvider,
    juce::ValueTree& state,
    std::function<void(const juce::String&)> printFunc)
{
    // If we're in REAPER and have the API available, use the REAPER pipeline
    if (isReaperHost && useReaperTimeline && reaperProvider && reaperProvider->isReaperApiAvailable())
    {
        return std::make_unique<ReaperMidiPipeline>(midiProcessor, *reaperProvider, state, printFunc);
    }

    // Otherwise, use the standard VST pipeline
    return std::make_unique<StandardMidiPipeline>(midiProcessor, state);
}