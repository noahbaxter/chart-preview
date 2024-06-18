#include "MidiProcessor.h"

void MidiProcessor::process(juce::MidiBuffer& midiMessages, uint startPositionInSamples, uint blockSizeInSamples)
{
    uint endPositionInSamples = startPositionInSamples + blockSizeInSamples;

    // Erase all events in the map within the range of the current block so changes are reflected
    // The problem is the buffer can shift unexpectedly, so we handle that by tracking the last processed sample
    if (endPositionInSamples >= lastProcessedSample)
    {
        // TODO: check if these lower_bounds work as expected
        auto lowerMEM = midiEventMap.lower_bound(std::max(startPositionInSamples, lastProcessedSample));
        auto upperMEM = midiEventMap.lower_bound(endPositionInSamples);
        midiEventMap.erase(lowerMEM, upperMEM);

        for (auto &noteStateMap : noteStateMaps)
        {
            auto lowerNSM = noteStateMap.lower_bound(std::max(startPositionInSamples, lastProcessedSample));
            auto upperNSM = noteStateMap.lower_bound(endPositionInSamples);
            noteStateMap.erase(lowerNSM, upperNSM);
        }
    }


    for (const auto &message : midiMessages)
    {
        int localMessagePositionInSamples = message.samplePosition;
        int globalMessagePositionInSamples = startPositionInSamples + localMessagePositionInSamples;
        auto midiMessage = message.getMessage();
        if (midiMessage.isNoteOn())
        {
            midiEventMap[globalMessagePositionInSamples].push_back(midiMessage);
            noteStateMaps[midiMessage.getNoteNumber()][globalMessagePositionInSamples] = true;
        }
        else if (midiMessage.isNoteOff())
        {
            noteStateMaps[midiMessage.getNoteNumber()][globalMessagePositionInSamples] = false;
        }
    }

    lastProcessedSample = std::max(endPositionInSamples, lastProcessedSample);
}