#pragma once

#include <JuceHeader.h>

class MidiProcessor
{

    public:
        void process(juce::MidiBuffer& midiMessages,
                     uint startPositionInSamples,
                     uint blockSizeInSamples);

        std::map<uint, std::vector<juce::MidiMessage>> midiEventMap;
        std::map<uint, std::array<bool, 128>> noteStateMap;
        uint lastProcessedSample = 0;

    private:
};