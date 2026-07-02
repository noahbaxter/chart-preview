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
#include "../../Utils/ChartTypes.h"

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

        return INVALID_COLUMN;
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

        return INVALID_COLUMN;
    }

    // Elite drums: kick(0) snare(1) hi-hat(2) L-crash(3) tom1(4) tom2(5) tom3(6)
    // ride(7) R-crash(8); 2x kick -> virtual column 9. Each lower difficulty is a
    // fixed -24 from Expert, so normalise to the Expert octave then map. Only called
    // with pitches already filtered to this skill (getEliteDrumPitchesForSkill).
    static uint getEliteDrumColumn(uint pitch, SkillLevel skill, bool kick2xEnabled)
    {
        int exp = (int)pitch + (4 - (int)skill) * 24;   // normalise to Expert octave
        switch (exp)
        {
            case 74: return 0;   // kick
            case 75: return 1;   // snare
            case 76: return 2;   // hi-hat
            case 77: return 3;   // left crash
            case 78: return 4;   // tom 1
            case 79: return 5;   // tom 2
            case 80: return 6;   // tom 3
            case 81: return 7;   // ride
            case 82: return 8;   // right crash
            case 73: return kick2xEnabled ? (uint)ELITE_KICK_2X_COLUMN : INVALID_COLUMN;
            default: return INVALID_COLUMN;
        }
    }

    // Inverse of getGuitarColumn: given a lane the user clicked, return the
    // MIDI pitch to write. col 0 = open, col 1-5 = green/red/yellow/blue/orange.
    // Returns -1 for invalid (col, skill) combinations.
    static int columnToGuitarPitch(SkillLevel skill, int col)
    {
        using Guitar = MidiPitchDefinitions::Guitar;
        switch (skill)
        {
            case SkillLevel::EXPERT:
                switch (col) {
                    case 0: return (int)Guitar::EXPERT_OPEN;
                    case 1: return (int)Guitar::EXPERT_GREEN;
                    case 2: return (int)Guitar::EXPERT_RED;
                    case 3: return (int)Guitar::EXPERT_YELLOW;
                    case 4: return (int)Guitar::EXPERT_BLUE;
                    case 5: return (int)Guitar::EXPERT_ORANGE;
                }
                break;
            case SkillLevel::HARD:
                switch (col) {
                    case 0: return (int)Guitar::HARD_OPEN;
                    case 1: return (int)Guitar::HARD_GREEN;
                    case 2: return (int)Guitar::HARD_RED;
                    case 3: return (int)Guitar::HARD_YELLOW;
                    case 4: return (int)Guitar::HARD_BLUE;
                    case 5: return (int)Guitar::HARD_ORANGE;
                }
                break;
            case SkillLevel::MEDIUM:
                switch (col) {
                    case 0: return (int)Guitar::MEDIUM_OPEN;
                    case 1: return (int)Guitar::MEDIUM_GREEN;
                    case 2: return (int)Guitar::MEDIUM_RED;
                    case 3: return (int)Guitar::MEDIUM_YELLOW;
                    case 4: return (int)Guitar::MEDIUM_BLUE;
                    case 5: return (int)Guitar::MEDIUM_ORANGE;
                }
                break;
            case SkillLevel::EASY:
                switch (col) {
                    case 0: return (int)Guitar::EASY_OPEN;
                    case 1: return (int)Guitar::EASY_GREEN;
                    case 2: return (int)Guitar::EASY_RED;
                    case 3: return (int)Guitar::EASY_YELLOW;
                    case 4: return (int)Guitar::EASY_BLUE;
                    case 5: return (int)Guitar::EASY_ORANGE;
                }
                break;
        }
        return -1;
    }

    // Inverse of getDrumColumn: given a lane the user clicked, return the
    // MIDI pitch to write. col 0 = kick (or 2x-kick if kick2x and EXPERT),
    // col 1-4 = red/yellow/blue/green pads. Returns -1 for invalid combos.
    static int columnToDrumPitch(SkillLevel skill, int col, bool kick2x)
    {
        using Drums = MidiPitchDefinitions::Drums;
        switch (skill)
        {
            case SkillLevel::EXPERT:
                switch (col) {
                    case 0: return kick2x ? (int)Drums::EXPERT_KICK_2X : (int)Drums::EXPERT_KICK;
                    case 1: return (int)Drums::EXPERT_RED;
                    case 2: return (int)Drums::EXPERT_YELLOW;
                    case 3: return (int)Drums::EXPERT_BLUE;
                    case 4: return (int)Drums::EXPERT_GREEN;
                }
                break;
            case SkillLevel::HARD:
                switch (col) {
                    case 0: return (int)Drums::HARD_KICK;
                    case 1: return (int)Drums::HARD_RED;
                    case 2: return (int)Drums::HARD_YELLOW;
                    case 3: return (int)Drums::HARD_BLUE;
                    case 4: return (int)Drums::HARD_GREEN;
                }
                break;
            case SkillLevel::MEDIUM:
                switch (col) {
                    case 0: return (int)Drums::MEDIUM_KICK;
                    case 1: return (int)Drums::MEDIUM_RED;
                    case 2: return (int)Drums::MEDIUM_YELLOW;
                    case 3: return (int)Drums::MEDIUM_BLUE;
                    case 4: return (int)Drums::MEDIUM_GREEN;
                }
                break;
            case SkillLevel::EASY:
                switch (col) {
                    case 0: return (int)Drums::EASY_KICK;
                    case 1: return (int)Drums::EASY_RED;
                    case 2: return (int)Drums::EASY_YELLOW;
                    case 3: return (int)Drums::EASY_BLUE;
                    case 4: return (int)Drums::EASY_GREEN;
                }
                break;
        }
        return -1;
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

    static std::vector<uint> getEliteDrumPitchesForSkill(SkillLevel skill)
    {
        using E = MidiPitchDefinitions::EliteDrums;
        switch (skill)
        {
            case SkillLevel::EASY:
                return {(uint)E::EASY_KICK,(uint)E::EASY_KICK_2X,(uint)E::EASY_SNARE,(uint)E::EASY_HIHAT,(uint)E::EASY_LCRASH,
                        (uint)E::EASY_TOM1,(uint)E::EASY_TOM2,(uint)E::EASY_TOM3,(uint)E::EASY_RIDE,(uint)E::EASY_RCRASH};
            case SkillLevel::MEDIUM:
                return {(uint)E::MEDIUM_KICK,(uint)E::MEDIUM_KICK_2X,(uint)E::MEDIUM_SNARE,(uint)E::MEDIUM_HIHAT,(uint)E::MEDIUM_LCRASH,
                        (uint)E::MEDIUM_TOM1,(uint)E::MEDIUM_TOM2,(uint)E::MEDIUM_TOM3,(uint)E::MEDIUM_RIDE,(uint)E::MEDIUM_RCRASH};
            case SkillLevel::HARD:
                return {(uint)E::HARD_KICK,(uint)E::HARD_KICK_2X,(uint)E::HARD_SNARE,(uint)E::HARD_HIHAT,(uint)E::HARD_LCRASH,
                        (uint)E::HARD_TOM1,(uint)E::HARD_TOM2,(uint)E::HARD_TOM3,(uint)E::HARD_RIDE,(uint)E::HARD_RCRASH};
            case SkillLevel::EXPERT:
                return {(uint)E::EXPERT_KICK,(uint)E::EXPERT_KICK_2X,(uint)E::EXPERT_SNARE,(uint)E::EXPERT_HIHAT,(uint)E::EXPERT_LCRASH,
                        (uint)E::EXPERT_TOM1,(uint)E::EXPERT_TOM2,(uint)E::EXPERT_TOM3,(uint)E::EXPERT_RIDE,(uint)E::EXPERT_RCRASH};
        }
        return {};
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

    static bool isKickLane(int lane) { return lane == DRUM_KICK_COLUMN || lane == DRUM_KICK_2X_COLUMN; }
    static bool is2xKickLane(int lane) { return lane == DRUM_KICK_2X_COLUMN; }
    enum class KickSide { None, Normal, Double };
    static KickSide getKickSide(int laneOrColumn)
    {
        if (laneOrColumn == DRUM_KICK_2X_COLUMN) return KickSide::Double;
        if (laneOrColumn == DRUM_KICK_COLUMN)    return KickSide::Normal;
        return KickSide::None;
    }

    static int resolveKickPitch(SkillLevel skill, int lane, bool kick2xEnabled)
    {
        if (is2xKickLane(lane))
            return kick2xEnabled ? (int)MidiPitchDefinitions::Drums::EXPERT_KICK_2X : -1;
        if (lane == DRUM_KICK_COLUMN)
            return columnToDrumPitch(skill, DRUM_KICK_COLUMN, false);
        return -1;
    }

    struct KickConflict { int pitch; int lane; };
    static KickConflict getConflictingKick(int pitch)
    {
        using Drums = MidiPitchDefinitions::Drums;
        if (pitch == (int)Drums::EXPERT_KICK_2X)
            return { (int)Drums::EXPERT_KICK, DRUM_KICK_COLUMN };
        return { (int)Drums::EXPERT_KICK_2X, DRUM_KICK_2X_COLUMN };
    }

    static bool isModifier(uint pitch, bool isElite = false)
    {
        using Guitar = MidiPitchDefinitions::Guitar;
        using Drums = MidiPitchDefinitions::Drums;

        // Elite drums own the whole 72-82 note octave (kick..R-crash), which overlaps
        // guitar's MEDIUM_HOPO (77) / MEDIUM_STRUM (78) etc. Those are ELITE NOTES, not
        // modifiers, so for elite only its genuine modifiers count. Star Power (104) is
        // handled; flam/hat/roll-lane modifiers live in the upper octave / 108-118 and
        // aren't parsed yet (they fall through as non-playable and are ignored).
        if (isElite)
            return pitch == (uint)MidiPitchDefinitions::EliteDrums::SP;

        // Guitar modifiers (all sustained)
        if (pitch == (uint)Guitar::SP ||
            pitch == (uint)Guitar::TAP ||
            pitch == (uint)Guitar::EASY_HOPO || pitch == (uint)Guitar::EASY_STRUM ||
            pitch == (uint)Guitar::MEDIUM_HOPO || pitch == (uint)Guitar::MEDIUM_STRUM ||
            pitch == (uint)Guitar::HARD_HOPO || pitch == (uint)Guitar::HARD_STRUM ||
            pitch == (uint)Guitar::EXPERT_HOPO || pitch == (uint)Guitar::EXPERT_STRUM ||
            pitch == (uint)Guitar::LANE_1 || pitch == (uint)Guitar::LANE_2)
        {
            return true;
        }

        // Drum modifiers (all sustained)
        if (pitch == (uint)Drums::SP ||
            pitch == (uint)Drums::TOM_YELLOW || pitch == (uint)Drums::TOM_BLUE || pitch == (uint)Drums::TOM_GREEN ||
            pitch == (uint)Drums::LANE_1 || pitch == (uint)Drums::LANE_2)
        {
            return true;
        }

        return false;
    }

private:
    static constexpr uint INVALID_COLUMN = uint(-1);
};
