/*
  ==============================================================================

    ChordAnalyzer.h
    Created: 18 Oct 2024
    Author:  Noah Baxter

    Analyzes chord formation and note state queries with tolerance windows.
    Handles chord detection, HOPO fixing for chords, and note held queries.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "MidiTypes.h"
#include "../../Utils/Utils.h"

class ChordAnalyzer
{
public:
    static bool isNoteHeld(uint pitch, PPQ position,
                           NoteStateMapArray& noteStateMapArray,
                           juce::CriticalSection& noteStateMapLock);

    static bool isNoteHeldWithTolerance(uint pitch, PPQ position,
                                        NoteStateMapArray& noteStateMapArray,
                                        juce::CriticalSection& noteStateMapLock);

    static bool isWithinChordTolerance(PPQ position1, PPQ position2);

    static void fixChordHOPOs(const std::vector<PPQ>& positions, SkillLevel skill,
                              NoteStateMapArray& noteStateMapArray,
                              juce::CriticalSection& noteStateMapLock);
};
