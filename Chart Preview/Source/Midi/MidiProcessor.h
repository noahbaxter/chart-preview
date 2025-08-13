#pragma once

#include <JuceHeader.h>
#include "../Utils.h"

class MidiProcessor
{
    public:
        void process(juce::MidiBuffer& midiMessages,
                     const juce::AudioPlayHead::PositionInfo& positionInfo,
                     uint blockSizeInSamples,
                     uint latencyInSamples,
                     double sampleRate);

        NoteStateMapArray noteStateMapArray;
        double lastProcessedPPQ = 0;

        void setLastProcessedPosition(const juce::AudioPlayHead::PositionInfo &positionInfo)
        {
            lastProcessedPPQ = positionInfo.getPpqPosition().orFallback(lastProcessedPPQ);
        }

    private:
        const uint maxNumMessagesPerBlock = 256;
        
        double estimatePPQFromSamples(uint samples,
                                      double bpm,
                                      double sampleRate);

};