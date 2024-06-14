#include "MidiProcessor.h"

void MidiProcessor::process(juce::MidiBuffer& midiMessages, uint startPositionInSamples, uint blockSizeInSamples)
{
    uint endPositionInSamples = startPositionInSamples + blockSizeInSamples;

    // Erase all events in the map within the range of the current block so changes are reflected
    // The problem is the buffer can shift unexpectedly, so we handle that by tracking the last processed sample
    if (endPositionInSamples >= lastProcessedSample)
    {
        auto lower = midiMap.lower_bound(std::max(startPositionInSamples, lastProcessedSample));
        auto upper = midiMap.lower_bound(endPositionInSamples);
        midiMap.erase(lower, upper);
    }


    for (const auto &message : midiMessages)
    {
        int localMessagePositionInSamples = message.samplePosition;
        int globalMessagePositionInSamples = startPositionInSamples + localMessagePositionInSamples;
        if (message.getMessage().isNoteOn())
        {
            midiMap[globalMessagePositionInSamples].push_back(message.getMessage());
        }
    }

    lastProcessedSample = std::max(endPositionInSamples, lastProcessedSample);
}