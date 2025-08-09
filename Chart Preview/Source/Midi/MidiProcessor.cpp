#include "MidiProcessor.h"

void MidiProcessor::process(juce::MidiBuffer& midiMessages, uint startPositionInSamples, uint blockSizeInSamples, uint latencyInSamples)
{
    uint endPositionInSamples = startPositionInSamples + blockSizeInSamples;

    // Erase all events in the map within the range of the current block so changes are reflected
    // The problem is the buffer can shift unexpectedly, so we handle that by tracking the last processed sample
    // Ableton be leik :/
    if (endPositionInSamples >= lastProcessedSample)
    {
        for (auto &noteStateMap : noteStateMapArray)
        {
            // OLD - build and maintain all midi events for the project
            // auto lower = noteStateMap.lower_bound(std::max(startPositionInSamples, lastProcessedSample));
            // auto upper = noteStateMap.lower_bound(endPositionInSamples);
            // noteStateMap.erase(lower, upper);

            // NEW - build and maintain only the midi events for the current block + adjascent events for each note
            auto lower = noteStateMap.upper_bound(std::max((int)(startPositionInSamples - latencyInSamples), 0));
            if (lower != noteStateMap.begin())
            {
                --lower;    // Keep last event of this note
                noteStateMap.erase(noteStateMap.begin(), lower);
            }
            // Remove all events after the end of the chart window, represented by the amount of latency
            auto upper = noteStateMap.upper_bound((int)(startPositionInSamples + latencyInSamples));
            noteStateMap.erase(upper, noteStateMap.end());
        }
    }

    uint numMessages = 0;
    for (const auto &message : midiMessages)
    {
        uint localMessagePositionInSamples = message.samplePosition;
        uint globalMessagePositionInSamples = startPositionInSamples + localMessagePositionInSamples;
        auto midiMessage = message.getMessage();

        if (midiMessage.isNoteOn() || midiMessage.isNoteOff())
        {
            // Prevent note offs from being at the same position as note ons
            if(midiMessage.isNoteOff() && globalMessagePositionInSamples > 0)
            {
                globalMessagePositionInSamples -= 1;
            }
            
            uint noteNumber = midiMessage.getNoteNumber();
            uint velocity = midiMessage.isNoteOn() ? midiMessage.getVelocity() : 0; // Note off MUST be velocity 0
            noteStateMapArray[noteNumber][globalMessagePositionInSamples] = velocity;
        }

        if (++numMessages >= maxNumMessagesPerBlock)
        {
            break;
        }
    }

    lastProcessedSample = std::max(endPositionInSamples, lastProcessedSample);
}