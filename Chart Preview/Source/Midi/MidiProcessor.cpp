#include "MidiProcessor.h"

void MidiProcessor::process(juce::MidiBuffer& midiMessages, uint startPositionInSamples, uint blockSizeInSamples)
{
    uint endPositionInSamples = startPositionInSamples + blockSizeInSamples;

    // Erase all events in the map within the range of the current block so changes are reflected
    // The problem is the buffer can shift unexpectedly, so we handle that by tracking the last processed sample
    // Ableton be leik :/
    if (endPositionInSamples >= lastProcessedSample)
    {
        for (auto &noteStateMap : noteStateMapArray)
        {
            auto lower = noteStateMap.lower_bound(std::max(startPositionInSamples, lastProcessedSample));
            auto upper = noteStateMap.lower_bound(endPositionInSamples);
            noteStateMap.erase(lower, upper);
        }
    }

    for (const auto &message : midiMessages)
    {
        uint localMessagePositionInSamples = message.samplePosition;
        uint globalMessagePositionInSamples = startPositionInSamples + localMessagePositionInSamples;
        auto midiMessage = message.getMessage();

        if (midiMessage.isNoteOn() || midiMessage.isNoteOff())
        {
            noteStateMapArray[midiMessage.getNoteNumber()][globalMessagePositionInSamples] = midiMessage.getVelocity();
        }
    }

    lastProcessedSample = std::max(endPositionInSamples, lastProcessedSample);
}