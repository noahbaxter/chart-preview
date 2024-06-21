#pragma once

#include <JuceHeader.h>
#include "../Utils.h"

class MidiProcessor
{
    public:
        void process(juce::MidiBuffer& midiMessages,
                     uint startPositionInSamples,
                     uint blockSizeInSamples);

        NoteStateMapArray noteStateMapArray;
        uint lastProcessedSample = 0;
};