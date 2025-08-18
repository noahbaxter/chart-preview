#pragma once
#include <JuceHeader.h>
#include "../Utils/Utils.h"

class MidiProcessor
{
public:
    void process(juce::MidiBuffer &midiMessages,
                 const juce::AudioPlayHead::PositionInfo &positionInfo,
                 uint blockSizeInSamples,
                 uint latencyInSamples,
                 double sampleRate);

    NoteStateMapArray noteStateMapArray;
    GridlineMap gridlineMap;
    mutable juce::CriticalSection gridlineMapLock;
    PPQ lastProcessedPPQ = 0.0;

    void setLastProcessedPosition(const juce::AudioPlayHead::PositionInfo &positionInfo)
    {
        lastProcessedPPQ = positionInfo.getPpqPosition().orFallback(lastProcessedPPQ);
    }


private:
    PPQ calculatePPQSegment(uint samples, double bpm, double sampleRate);
    void cleanupOldEvents(PPQ startPPQ, PPQ endPPQ, PPQ latencyPPQ);
    
    PPQ lastTimeSignatureChangePPQ = 0.0;
    uint lastTimeSignatureNumerator = 4;
    uint lastTimeSignatureDenominator = 4;
    void buildGridlineMap(PPQ startPPQ, PPQ endPPQ, uint initialTimeSignatureNumerator, uint initialTimeSignatureDenominator);
    
    const uint maxNumMessagesPerBlock = 256;
    void processMidiMessages(juce::MidiBuffer &midiMessages, PPQ startPPQ, double sampleRate, double bpm);
    void processNoteMessage(const juce::MidiMessage &midiMessage, PPQ messagePPQ);
};