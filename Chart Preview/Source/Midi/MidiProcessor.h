#pragma once

#include <JuceHeader.h>

class MidiProcessor
{

    public:
        void process(juce::MidiBuffer& midiMessages,
                     uint startPositionInSamples,
                     uint blockSizeInSamples);

        std::map<uint, std::vector<juce::MidiMessage>> midiEventMap;
        std::array<std::map<uint, bool>, 128> noteStateMaps;
        uint lastProcessedSample = 0;

    private:
};