/*
  ==============================================================================

    MidiUtility.h
    Created: 29 Aug 2024
    Author:  Noah Baxter

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Utils/Utils.h"

class MidiUtility
{
public:
    // Column calculation functions
    static uint getGuitarGemColumn(uint pitch, juce::ValueTree &state);
    static uint getDrumGemColumn(uint pitch, juce::ValueTree &state);
    
    // Note state query functions
    static bool isNoteHeld(uint pitch, PPQ position, NoteStateMapArray &noteStateMapArray, juce::CriticalSection &noteStateMapLock);
    static bool isNoteHeldWithTolerance(uint pitch, PPQ position, NoteStateMapArray &noteStateMapArray, juce::CriticalSection &noteStateMapLock);
    
    // Gem type calculation functions
    static Gem getGuitarGemType(uint pitch, PPQ position, juce::ValueTree &state, NoteStateMapArray &noteStateMapArray, juce::CriticalSection &noteStateMapLock);
    static Gem getDrumGemType(uint pitch, PPQ position, Dynamic dynamic, juce::ValueTree &state, NoteStateMapArray &noteStateMapArray, juce::CriticalSection &noteStateMapLock);
    static bool shouldBeAutoHOPO(uint pitch, PPQ position, juce::ValueTree &state, NoteStateMapArray &noteStateMapArray, juce::CriticalSection &noteStateMapLock);
    
    // Helper utility functions
    static std::vector<uint> getGuitarPitchesForSkill(SkillLevel skill);
    static std::vector<uint> getDrumPitchesForSkill(SkillLevel skill);
    static bool isDrumKick(uint pitch);
    static bool isWithinChordTolerance(PPQ position1, PPQ position2);
    static bool isSustainedModifierPitch(uint pitch);
    
    // Lane detection functions
    static std::vector<SustainEvent> detectLanes(uint laneType, PPQ startPPQ, PPQ endPPQ, uint laneVelocity,
                                                  juce::ValueTree &state, NoteStateMapArray &noteStateMapArray,
                                                  juce::CriticalSection &noteStateMapLock);

    // Chord HOPO fixing (ensures chords are never mixed HOPO/strum)
    static void fixChordHOPOs(const std::vector<PPQ>& positions, SkillLevel skill,
                              NoteStateMapArray &noteStateMapArray,
                              juce::CriticalSection &noteStateMapLock);

    // Gem appearance utility
    static Gem getDrumGlyph(bool cymbal, bool dynamicsEnabled, Dynamic dynamic)
    {
        if (dynamicsEnabled)
        {
            switch (dynamic)
            {
            case Dynamic::GHOST:
                return cymbal ? Gem::CYM_GHOST : Gem::HOPO_GHOST;
                break;
            case Dynamic::ACCENT:
                return cymbal ? Gem::CYM_ACCENT : Gem::TAP_ACCENT;
                break;
            default:
                return cymbal ? Gem::CYM : Gem::NOTE;
                break;
            }
        }
        else
        {
            return cymbal ? Gem::CYM : Gem::NOTE;
        }
    }
};