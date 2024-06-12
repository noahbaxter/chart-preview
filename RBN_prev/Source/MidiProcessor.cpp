#include "MidiProcessor.h"

void MidiProcessor::process(juce::MidiBuffer& midiMessages, int globalPlayheadPositionInSamples, int blockSizeInSamples)
{
    // Reset all map indices within the block
    auto lower = midiMap.lower_bound(globalPlayheadPositionInSamples);
    auto upper = midiMap.upper_bound(globalPlayheadPositionInSamples + blockSizeInSamples);
    midiMap.erase(lower, upper);

    // Store all events in the map
    juce::MidiBuffer::Iterator iter(midiMessages);
    juce::MidiMessage message;
    int localMessagePositionInSamples; // The sample position in the buffer where this event happens

    while (iter.getNextEvent(message, localMessagePositionInSamples))
    {
        int globalMessagePositionInSamples = globalPlayheadPositionInSamples + localMessagePositionInSamples;
        if (message.isNoteOn())
        {
            midiMap[globalMessagePositionInSamples].push_back(message);
        }
        
    }
}