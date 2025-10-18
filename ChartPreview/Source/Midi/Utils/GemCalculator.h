/*
  ==============================================================================

    GemCalculator.h
    Created: 18 Oct 2024
    Author:  Noah Baxter

    Calculates gem appearance (type) based on note state and modifiers.
    Handles guitar and drum gem types with auto-HOPO detection logic.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "MidiTypes.h"
#include "../../Utils/Utils.h"

class GemCalculator
{
public:
    static Gem getGuitarGemType(uint pitch, PPQ position, juce::ValueTree& state,
                                NoteStateMapArray& noteStateMapArray,
                                juce::CriticalSection& noteStateMapLock);

    static Gem getDrumGemType(uint pitch, PPQ position, Dynamic dynamic,
                              juce::ValueTree& state,
                              NoteStateMapArray& noteStateMapArray,
                              juce::CriticalSection& noteStateMapLock);

    static bool shouldBeAutoHOPO(uint pitch, PPQ position, juce::ValueTree& state,
                                 NoteStateMapArray& noteStateMapArray,
                                 juce::CriticalSection& noteStateMapLock);

    static Gem getDrumGlyph(bool cymbal, bool dynamicsEnabled, Dynamic dynamic);
};
