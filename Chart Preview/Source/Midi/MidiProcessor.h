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

    PPQ calculatePPQPosition(PPQ startPPQ, uint sampleOffset, double sampleRate);

private:
    PPQ lastTimeSignatureChangePPQ = 0.0;
    const uint maxNumMessagesPerBlock = 256;

    std::map<PPQ, double> tempoChanges; // PPQ position, new BPM
    std::map<PPQ, std::pair<uint, uint>> timeSignatureChanges; // PPQ position, new time signature

    PPQ buildTimingMap(juce::MidiBuffer &midiMessages, uint bufferSizeInSamples, PPQ startPPQ, double initialBpm, uint initialTimeSignatureNumerator, uint initialTimeSignatureDenominator, double sampleRate);
    PPQ calculatePPQSegment(uint samples, double bpm, double sampleRate);

    void cleanupOldEvents(PPQ startPPQ, PPQ endPPQ, PPQ latencyPPQ);

    void buildGridLineMap(PPQ startPPQ, PPQ endPPQ);
    void generateGridLines(PPQ startPPQ, PPQ endPPQ, uint timeSignatureNumerator, uint timeSignatureDenominator);

    void buildNoteStateMap(juce::MidiBuffer &midiMessages, PPQ startPPQ, double sampleRate);
    void processNoteMessage(const juce::MidiMessage &midiMessage, PPQ messagePPQ);
};