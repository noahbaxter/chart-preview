/*
  ==============================================================================

    NoteProcessor.h
    Converts raw MIDI notes to visual gem types

    Handles:
    - Modifier note processing (HOPO, star power, lanes, etc.)
    - Playable note processing (frets, drums, etc.)
    - Guitar chord analysis and HOPO fixing
    - Drum dynamics handling

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Providers/REAPER/MidiCache.h"
#include "MidiProcessor.h"
#include "../../Utils/Utils.h"

class NoteProcessor
{
public:
    NoteProcessor() = default;

    // Process all modifier notes (HOPO, star power, lanes, etc.)
    void processModifierNotes(
        const std::vector<MidiCache::CachedNote>& notes,
        NoteStateMapArray& noteStateMapArray,
        juce::CriticalSection& noteStateMapLock,
        juce::ValueTree& state);

    // Process all playable notes (frets, drums, etc.) and apply gem type calculation
    void processPlayableNotes(
        const std::vector<MidiCache::CachedNote>& notes,
        NoteStateMapArray& noteStateMapArray,
        juce::CriticalSection& noteStateMapLock,
        MidiProcessor& midiProcessor,
        juce::ValueTree& state,
        double bpm,
        double sampleRate);

private:
    // Caller must hold noteStateMapLock!
    void addNoteToMap(NoteStateMapArray& noteStateMapArray, uint pitch, PPQ startPPQ, const NoteData& data);
    void addNoteOffToMap(NoteStateMapArray& noteStateMapArray, uint pitch, PPQ endPPQ);
};
