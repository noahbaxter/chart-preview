#pragma once

#include <JuceHeader.h>

class MidiProcessor
{

    public:
        void process(juce::MidiBuffer& midiMessages,
                     uint startPositionInSamples,
                     uint blockSizeInSamples);

        std::map<int, std::vector<juce::MidiMessage>> midiMap;
        uint lastProcessedSample = 0;

    private:
};