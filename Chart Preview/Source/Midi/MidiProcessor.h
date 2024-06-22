#pragma once

#include <JuceHeader.h>
#include "../Utils.h"

class MidiProcessor
{
    public:
        void process(juce::MidiBuffer& midiMessages,
                     uint startPositionInSamples,
                     uint blockSizeInSamples,
                     uint latencyInSamples);

        NoteStateMapArray noteStateMapArray;
        uint lastProcessedSample = 0;

    private:
        // The maximum number of messages to process per block
        const uint maxNumMessages = 256;
};