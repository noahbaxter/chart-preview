#include "MidiProcessor.h"

void MidiProcessor::process(juce::MidiBuffer &midiMessages, uint startPositionInSamples, uint endPositionInSamples, uint latencyInSamples)
{
    // Erase all events in the map within the range of the current block so changes are reflected
    for (auto &noteStateMap : noteStateMapArray)
    {
        // OLD - build and maintain all midi events for the project
        // auto lower = noteStateMap.lower_bound(startPositionInSamples);
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

    uint numMessages = 0;
    for (const auto &message : midiMessages)
    {
        uint localMessagePositionInSamples = message.samplePosition;
        uint globalMessagePositionInSamples = startPositionInSamples + localMessagePositionInSamples;
        auto midiMessage = message.getMessage();

        if (midiMessage.isNoteOn() || midiMessage.isNoteOff())
        {
            uint noteNumber = midiMessage.getNoteNumber();
            uint velocity = midiMessage.isNoteOn() ? midiMessage.getVelocity() : 0; // Note off MUST be velocity 0
            noteStateMapArray[noteNumber][globalMessagePositionInSamples] = velocity;
        }

        numMessages++;
        if (numMessages >= maxNumMessages)
        {
            break;
        }
    }
}