#pragma once

#include <JuceHeader.h>

class MidiProcessor
{

    public:
        void process(juce::MidiBuffer& midiMessages, 
                     int globalPlayheadPositionInSamples,
                     int blockSizeInSamples);

        std::map<int, std::vector<juce::MidiMessage>> midiMap;

    // private:
};