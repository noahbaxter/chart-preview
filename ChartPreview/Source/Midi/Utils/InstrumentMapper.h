/*
  ==============================================================================

    InstrumentMapper.h
    Header-only utility for MIDI pitch mapping and instrument classification

    Maps MIDI pitches to visual columns and skill-level-specific pitch sets.
    Handles both Guitar and Drum instruments with their respective note mappings.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "MidiTypes.h"
#include "../../Utils/Utils.h"

class InstrumentMapper
{
public:
    // Column mapping helpers
    static uint getGuitarColumn(uint pitch, SkillLevel skill)
    {
        using Guitar = MidiPitchDefinitions::Guitar;
        Guitar note = (Guitar)pitch;

        // Open notes - column 0
        if ((note == Guitar::EASY_OPEN && skill == SkillLevel::EASY) ||
            (note == Guitar::MEDIUM_OPEN && skill == SkillLevel::MEDIUM) ||
            (note == Guitar::HARD_OPEN && skill == SkillLevel::HARD) ||
            (note == Guitar::EXPERT_OPEN && skill == SkillLevel::EXPERT))
        {
            return 0;
        }
        // Green notes - column 1
        else if ((note == Guitar::EASY_GREEN && skill == SkillLevel::EASY) ||
                 (note == Guitar::MEDIUM_GREEN && skill == SkillLevel::MEDIUM) ||
                 (note == Guitar::HARD_GREEN && skill == SkillLevel::HARD) ||
                 (note == Guitar::EXPERT_GREEN && skill == SkillLevel::EXPERT))
        {
            return 1;
        }
        // Red notes - column 2
        else if ((note == Guitar::EASY_RED && skill == SkillLevel::EASY) ||
                 (note == Guitar::MEDIUM_RED && skill == SkillLevel::MEDIUM) ||
                 (note == Guitar::HARD_RED && skill == SkillLevel::HARD) ||
                 (note == Guitar::EXPERT_RED && skill == SkillLevel::EXPERT))
        {
            return 2;
        }
        // Yellow notes - column 3
        else if ((note == Guitar::EASY_YELLOW && skill == SkillLevel::EASY) ||
                 (note == Guitar::MEDIUM_YELLOW && skill == SkillLevel::MEDIUM) ||
                 (note == Guitar::HARD_YELLOW && skill == SkillLevel::HARD) ||
                 (note == Guitar::EXPERT_YELLOW && skill == SkillLevel::EXPERT))
        {
            return 3;
        }
        // Blue notes - column 4
        else if ((note == Guitar::EASY_BLUE && skill == SkillLevel::EASY) ||
                 (note == Guitar::MEDIUM_BLUE && skill == SkillLevel::MEDIUM) ||
                 (note == Guitar::HARD_BLUE && skill == SkillLevel::HARD) ||
                 (note == Guitar::EXPERT_BLUE && skill == SkillLevel::EXPERT))
        {
            return 4;
        }
        // Orange notes - column 5
        else if ((note == Guitar::EASY_ORANGE && skill == SkillLevel::EASY) ||
                 (note == Guitar::MEDIUM_ORANGE && skill == SkillLevel::MEDIUM) ||
                 (note == Guitar::HARD_ORANGE && skill == SkillLevel::HARD) ||
                 (note == Guitar::EXPERT_ORANGE && skill == SkillLevel::EXPERT))
        {
            return 5;
        }

        return LANE_COUNT; // Invalid/unknown pitch
    }

    static uint getDrumColumn(uint pitch, SkillLevel skill, bool kick2xEnabled)
    {
        using Drums = MidiPitchDefinitions::Drums;
        Drums note = (Drums)pitch;

        if ((note == Drums::EASY_KICK && skill == SkillLevel::EASY) ||
            (note == Drums::MEDIUM_KICK && skill == SkillLevel::MEDIUM) ||
            (note == Drums::HARD_KICK && skill == SkillLevel::HARD) ||
            (note == Drums::EXPERT_KICK && skill == SkillLevel::EXPERT))
        {
            return 0;
        }
        else if ((note == Drums::EASY_RED && skill == SkillLevel::EASY) ||
                 (note == Drums::MEDIUM_RED && skill == SkillLevel::MEDIUM) ||
                 (note == Drums::HARD_RED && skill == SkillLevel::HARD) ||
                 (note == Drums::EXPERT_RED && skill == SkillLevel::EXPERT))
        {
            return 1;
        }
        else if ((note == Drums::EASY_YELLOW && skill == SkillLevel::EASY) ||
                 (note == Drums::MEDIUM_YELLOW && skill == SkillLevel::MEDIUM) ||
                 (note == Drums::HARD_YELLOW && skill == SkillLevel::HARD) ||
                 (note == Drums::EXPERT_YELLOW && skill == SkillLevel::EXPERT))
        {
            return 2;
        }
        else if ((note == Drums::EASY_BLUE && skill == SkillLevel::EASY) ||
                 (note == Drums::MEDIUM_BLUE && skill == SkillLevel::MEDIUM) ||
                 (note == Drums::HARD_BLUE && skill == SkillLevel::HARD) ||
                 (note == Drums::EXPERT_BLUE && skill == SkillLevel::EXPERT))
        {
            return 3;
        }
        else if ((note == Drums::EASY_GREEN && skill == SkillLevel::EASY) ||
                 (note == Drums::MEDIUM_GREEN && skill == SkillLevel::MEDIUM) ||
                 (note == Drums::HARD_GREEN && skill == SkillLevel::HARD) ||
                 (note == Drums::EXPERT_GREEN && skill == SkillLevel::EXPERT))
        {
            return 4;
        }
        else if (kick2xEnabled && note == Drums::EXPERT_KICK_2X && skill == SkillLevel::EXPERT)
        {
            return 6;
        }

        return LANE_COUNT; // Invalid/unknown pitch
    }

    // Playable pitch helpers
    static std::vector<uint> getGuitarPitchesForSkill(SkillLevel skill)
    {
        using Guitar = MidiPitchDefinitions::Guitar;
        switch (skill)
        {
            case SkillLevel::EASY:
                return {(uint)Guitar::EASY_OPEN, (uint)Guitar::EASY_GREEN, (uint)Guitar::EASY_RED, (uint)Guitar::EASY_YELLOW, (uint)Guitar::EASY_BLUE, (uint)Guitar::EASY_ORANGE};
            case SkillLevel::MEDIUM:
                return {(uint)Guitar::MEDIUM_OPEN, (uint)Guitar::MEDIUM_GREEN, (uint)Guitar::MEDIUM_RED, (uint)Guitar::MEDIUM_YELLOW, (uint)Guitar::MEDIUM_BLUE, (uint)Guitar::MEDIUM_ORANGE};
            case SkillLevel::HARD:
                return {(uint)Guitar::HARD_OPEN, (uint)Guitar::HARD_GREEN, (uint)Guitar::HARD_RED, (uint)Guitar::HARD_YELLOW, (uint)Guitar::HARD_BLUE, (uint)Guitar::HARD_ORANGE};
            case SkillLevel::EXPERT:
                return {(uint)Guitar::EXPERT_OPEN, (uint)Guitar::EXPERT_GREEN, (uint)Guitar::EXPERT_RED, (uint)Guitar::EXPERT_YELLOW, (uint)Guitar::EXPERT_BLUE, (uint)Guitar::EXPERT_ORANGE};
        }
        return {}; // Empty vector for invalid skill level
    }

    static std::vector<uint> getDrumPitchesForSkill(SkillLevel skill)
    {
        using Drums = MidiPitchDefinitions::Drums;
        switch (skill)
        {
            case SkillLevel::EASY:
                return {(uint)Drums::EASY_KICK, (uint)Drums::EASY_RED, (uint)Drums::EASY_YELLOW, (uint)Drums::EASY_BLUE, (uint)Drums::EASY_GREEN};
            case SkillLevel::MEDIUM:
                return {(uint)Drums::MEDIUM_KICK, (uint)Drums::MEDIUM_RED, (uint)Drums::MEDIUM_YELLOW, (uint)Drums::MEDIUM_BLUE, (uint)Drums::MEDIUM_GREEN};
            case SkillLevel::HARD:
                return {(uint)Drums::HARD_KICK, (uint)Drums::HARD_RED, (uint)Drums::HARD_YELLOW, (uint)Drums::HARD_BLUE, (uint)Drums::HARD_GREEN};
            case SkillLevel::EXPERT:
                return {(uint)Drums::EXPERT_KICK, (uint)Drums::EXPERT_RED, (uint)Drums::EXPERT_YELLOW, (uint)Drums::EXPERT_BLUE, (uint)Drums::EXPERT_GREEN, (uint)Drums::EXPERT_KICK_2X};
        }
        return {}; // Empty vector for invalid skill level
    }

    // Modifier pitch helpers
    static std::vector<uint> getGuitarModifierPitchesForSkill(SkillLevel skill)
    {
        using Guitar = MidiPitchDefinitions::Guitar;
        switch (skill)
        {
            case SkillLevel::EXPERT:
                return {(uint)Guitar::EXPERT_HOPO, (uint)Guitar::EXPERT_STRUM,
                       (uint)Guitar::TAP, (uint)Guitar::SP,
                       (uint)Guitar::LANE_1, (uint)Guitar::LANE_2};
            case SkillLevel::HARD:
                return {(uint)Guitar::HARD_HOPO, (uint)Guitar::HARD_STRUM,
                       (uint)Guitar::TAP, (uint)Guitar::SP,
                       (uint)Guitar::LANE_1, (uint)Guitar::LANE_2};
            case SkillLevel::MEDIUM:
                return {(uint)Guitar::MEDIUM_HOPO, (uint)Guitar::MEDIUM_STRUM,
                       (uint)Guitar::TAP, (uint)Guitar::SP,
                       (uint)Guitar::LANE_1, (uint)Guitar::LANE_2};
            case SkillLevel::EASY:
                return {(uint)Guitar::EASY_HOPO, (uint)Guitar::EASY_STRUM,
                       (uint)Guitar::TAP, (uint)Guitar::SP,
                       (uint)Guitar::LANE_1, (uint)Guitar::LANE_2};
        }
        return {};
    }

    static std::vector<uint> getDrumModifierPitches()
    {
        using Drums = MidiPitchDefinitions::Drums;
        return {(uint)Drums::TOM_YELLOW, (uint)Drums::TOM_BLUE, (uint)Drums::TOM_GREEN,
               (uint)Drums::SP, (uint)Drums::LANE_1, (uint)Drums::LANE_2};
    }

    // Pitch classification helpers
    static bool isDrumKick(uint pitch)
    {
        using Drums = MidiPitchDefinitions::Drums;
        Drums note = (Drums)pitch;
        return (note == Drums::EASY_KICK ||
                note == Drums::MEDIUM_KICK ||
                note == Drums::HARD_KICK ||
                note == Drums::EXPERT_KICK ||
                note == Drums::EXPERT_KICK_2X);
    }

    static bool isSustainedModifierPitch(uint pitch)
    {
        using Guitar = MidiPitchDefinitions::Guitar;
        using Drums = MidiPitchDefinitions::Drums;

        // Guitar modifiers (all sustained)
        if (pitch == (uint)Guitar::SP ||
            pitch == (uint)Guitar::TAP ||
            pitch == (uint)Guitar::EXPERT_STRUM || pitch == (uint)Guitar::EXPERT_HOPO ||
            pitch == (uint)Guitar::HARD_STRUM || pitch == (uint)Guitar::HARD_HOPO ||
            pitch == (uint)Guitar::MEDIUM_STRUM || pitch == (uint)Guitar::MEDIUM_HOPO ||
            pitch == (uint)Guitar::EASY_STRUM || pitch == (uint)Guitar::EASY_HOPO ||
            pitch == (uint)Guitar::LANE_1 || pitch == (uint)Guitar::LANE_2)
        {
            return true;
        }

        // Drum modifiers (all sustained)
        if (pitch == (uint)Drums::SP ||
            pitch == (uint)Drums::TOM_GREEN || pitch == (uint)Drums::TOM_BLUE || pitch == (uint)Drums::TOM_YELLOW ||
            pitch == (uint)Drums::LANE_1 || pitch == (uint)Drums::LANE_2)
        {
            return true;
        }

        return false;
    }

private:
    static constexpr size_t LANE_COUNT = 7;  // Number of playable lanes (0-6)
};
