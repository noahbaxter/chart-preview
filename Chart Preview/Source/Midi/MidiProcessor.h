#pragma once

#include <JuceHeader.h>
#include "../Utils/Utils.h"

class MidiProcessor
{
    public:
        void process(juce::MidiBuffer& midiMessages,
                     const juce::AudioPlayHead::PositionInfo& positionInfo,
                     uint blockSizeInSamples,
                     uint latencyInSamples,
                     double sampleRate);

        NoteStateMapArray noteStateMapArray;
        PPQ lastProcessedPPQ = 0.0;

        void setLastProcessedPosition(const juce::AudioPlayHead::PositionInfo &positionInfo)
        {
            lastProcessedPPQ = positionInfo.getPpqPosition().orFallback(lastProcessedPPQ);
        }

    private:
        const uint maxNumMessagesPerBlock = 256;
        
                PPQ estimatePPQFromSamples(uint samples, double bpm, double sampleRate);

};