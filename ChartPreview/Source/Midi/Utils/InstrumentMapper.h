/*
  ==============================================================================

    InstrumentMapper.h
    Created: 18 Oct 2024
    Author:  Noah Baxter

    Maps MIDI pitches to visual columns and skill-level-specific pitch sets.
    Handles both Guitar and Drum instruments with their respective note mappings.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Utils/Utils.h"

class InstrumentMapper
{
public:
    static uint getGuitarColumn(uint pitch, SkillLevel skill);
    static uint getDrumColumn(uint pitch, SkillLevel skill, bool kick2xEnabled);

    static std::vector<uint> getGuitarPitchesForSkill(SkillLevel skill);
    static std::vector<uint> getDrumPitchesForSkill(SkillLevel skill);

    static bool isDrumKick(uint pitch);
    static bool isSustainedModifierPitch(uint pitch);
};
