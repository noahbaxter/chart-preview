#pragma once
#include <JuceHeader.h>
#include "../Utils/Utils.h"
#include "MidiUtility.h"

// Forward declaration to avoid circular dependency
class ReaperMidiProvider;

class MidiProcessor
{
public:
    MidiProcessor(juce::ValueTree &state);
    
    void process(juce::MidiBuffer &midiMessages,
                 const juce::AudioPlayHead::PositionInfo &positionInfo,
                 uint blockSizeInSamples,
                 uint latencyInSamples,
                 double sampleRate);

    // Process lookahead MIDI data from REAPER timeline for scrubbing/preview
    void processLookaheadMidi(ReaperMidiProvider* reaperMidiProvider,
                              PPQ currentPosition,
                              PPQ lookaheadRange,
                              double sampleRate,
                              double bpm);

    NoteStateMapArray noteStateMapArray;
    GridlineMap gridlineMap;
    mutable juce::CriticalSection gridlineMapLock;
    mutable juce::CriticalSection noteStateMapLock;
    PPQ lastProcessedPPQ = 0.0;

    void setLastProcessedPosition(const juce::AudioPlayHead::PositionInfo &positionInfo)
    {
        lastProcessedPPQ = positionInfo.getPpqPosition().orFallback(lastProcessedPPQ);
    }

    // Set the current visual window bounds to prevent cleanup from deleting visible events
    void setVisualWindowBounds(PPQ startPPQ, PPQ endPPQ)
    {
        const juce::ScopedLock lock(visualWindowLock);
        visualWindowStartPPQ = startPPQ;
        visualWindowEndPPQ = endPPQ;
    }

    // Recalculate gem types for all existing notes (called when settings change)
    void refreshMidiDisplay();


private:
    juce::ValueTree &state;
    
    PPQ calculatePPQSegment(uint samples, double bpm, double sampleRate);
    void cleanupOldEvents(PPQ startPPQ, PPQ endPPQ, PPQ latencyPPQ);
    
    PPQ lastTimeSignatureChangePPQ = 0.0;
    uint lastTimeSignatureNumerator = 4;
    uint lastTimeSignatureDenominator = 4;
    void buildGridlineMap(PPQ startPPQ, PPQ endPPQ, uint initialTimeSignatureNumerator, uint initialTimeSignatureDenominator);
    
    const uint maxNumMessagesPerBlock = 256;
    void processMidiMessages(juce::MidiBuffer &midiMessages, PPQ startPPQ, double sampleRate, double bpm);
    void processNoteMessage(const juce::MidiMessage &midiMessage, PPQ messagePPQ);
    bool isChordFormed(uint pitch, PPQ position);
    void fixChordHOPOs(uint pitch, PPQ position);
    
    // HOPO calculation moved from MidiInterpreter
    bool isNoteHeld(uint pitch, PPQ position);
    uint getGuitarGemColumn(uint pitch);
    Gem getGuitarGemType(uint pitch, PPQ position);
    uint getDrumGemColumn(uint pitch);
    Gem getDrumGemType(uint pitch, PPQ position, Dynamic dynamic);
    
    // Visual window bounds for conservative cleanup
    PPQ visualWindowStartPPQ = PPQ(0.0);
    PPQ visualWindowEndPPQ = PPQ(0.0);
    mutable juce::CriticalSection visualWindowLock;
};