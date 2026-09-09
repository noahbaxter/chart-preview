#include "test_helpers.h"

using Guitar = MidiPitchDefinitions::Guitar;
using Drums = MidiPitchDefinitions::Drums;
using EliteDrums = MidiPitchDefinitions::EliteDrums;

// ============================================================================
// getGuitarColumn

TEST_CASE("InstrumentMapper - getGuitarColumn", "[instrument_mapper][guitar]")
{
    SECTION("EXPERT skill → all 6 columns")
    {
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::EXPERT_OPEN, SkillLevel::EXPERT) == 0);
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::EXPERT_GREEN, SkillLevel::EXPERT) == 1);
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::EXPERT_RED, SkillLevel::EXPERT) == 2);
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::EXPERT_YELLOW, SkillLevel::EXPERT) == 3);
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::EXPERT_BLUE, SkillLevel::EXPERT) == 4);
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::EXPERT_ORANGE, SkillLevel::EXPERT) == 5);
    }

    SECTION("HARD skill → all 6 columns")
    {
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::HARD_OPEN, SkillLevel::HARD) == 0);
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::HARD_GREEN, SkillLevel::HARD) == 1);
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::HARD_RED, SkillLevel::HARD) == 2);
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::HARD_YELLOW, SkillLevel::HARD) == 3);
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::HARD_BLUE, SkillLevel::HARD) == 4);
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::HARD_ORANGE, SkillLevel::HARD) == 5);
    }

    SECTION("MEDIUM skill → all 6 columns")
    {
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::MEDIUM_OPEN, SkillLevel::MEDIUM) == 0);
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::MEDIUM_GREEN, SkillLevel::MEDIUM) == 1);
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::MEDIUM_RED, SkillLevel::MEDIUM) == 2);
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::MEDIUM_YELLOW, SkillLevel::MEDIUM) == 3);
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::MEDIUM_BLUE, SkillLevel::MEDIUM) == 4);
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::MEDIUM_ORANGE, SkillLevel::MEDIUM) == 5);
    }

    SECTION("EASY skill → all 6 columns")
    {
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::EASY_OPEN, SkillLevel::EASY) == 0);
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::EASY_GREEN, SkillLevel::EASY) == 1);
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::EASY_RED, SkillLevel::EASY) == 2);
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::EASY_YELLOW, SkillLevel::EASY) == 3);
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::EASY_BLUE, SkillLevel::EASY) == 4);
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::EASY_ORANGE, SkillLevel::EASY) == 5);
    }

    SECTION("invalid pitch → INVALID_COLUMN")
    {
        REQUIRE(InstrumentMapper::getGuitarColumn(255, SkillLevel::EXPERT) == uint(-1));
    }

    SECTION("wrong skill for pitch → INVALID_COLUMN")
    {
        // EXPERT_GREEN with EASY skill should not match
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::EXPERT_GREEN, SkillLevel::EASY) == uint(-1));
        REQUIRE(InstrumentMapper::getGuitarColumn((uint)Guitar::EASY_GREEN, SkillLevel::EXPERT) == uint(-1));
    }
}

// ============================================================================
// getDrumColumn

TEST_CASE("InstrumentMapper - getDrumColumn", "[instrument_mapper][drums]")
{
    SECTION("EXPERT skill → all 5 columns")
    {
        REQUIRE(InstrumentMapper::getDrumColumn((uint)Drums::EXPERT_KICK, SkillLevel::EXPERT, false) == 0);
        REQUIRE(InstrumentMapper::getDrumColumn((uint)Drums::EXPERT_RED, SkillLevel::EXPERT, false) == 1);
        REQUIRE(InstrumentMapper::getDrumColumn((uint)Drums::EXPERT_YELLOW, SkillLevel::EXPERT, false) == 2);
        REQUIRE(InstrumentMapper::getDrumColumn((uint)Drums::EXPERT_BLUE, SkillLevel::EXPERT, false) == 3);
        REQUIRE(InstrumentMapper::getDrumColumn((uint)Drums::EXPERT_GREEN, SkillLevel::EXPERT, false) == 4);
    }

    SECTION("HARD skill → all 5 columns")
    {
        REQUIRE(InstrumentMapper::getDrumColumn((uint)Drums::HARD_KICK, SkillLevel::HARD, false) == 0);
        REQUIRE(InstrumentMapper::getDrumColumn((uint)Drums::HARD_RED, SkillLevel::HARD, false) == 1);
        REQUIRE(InstrumentMapper::getDrumColumn((uint)Drums::HARD_YELLOW, SkillLevel::HARD, false) == 2);
        REQUIRE(InstrumentMapper::getDrumColumn((uint)Drums::HARD_BLUE, SkillLevel::HARD, false) == 3);
        REQUIRE(InstrumentMapper::getDrumColumn((uint)Drums::HARD_GREEN, SkillLevel::HARD, false) == 4);
    }

    SECTION("MEDIUM skill → all 5 columns")
    {
        REQUIRE(InstrumentMapper::getDrumColumn((uint)Drums::MEDIUM_KICK, SkillLevel::MEDIUM, false) == 0);
        REQUIRE(InstrumentMapper::getDrumColumn((uint)Drums::MEDIUM_RED, SkillLevel::MEDIUM, false) == 1);
        REQUIRE(InstrumentMapper::getDrumColumn((uint)Drums::MEDIUM_YELLOW, SkillLevel::MEDIUM, false) == 2);
        REQUIRE(InstrumentMapper::getDrumColumn((uint)Drums::MEDIUM_BLUE, SkillLevel::MEDIUM, false) == 3);
        REQUIRE(InstrumentMapper::getDrumColumn((uint)Drums::MEDIUM_GREEN, SkillLevel::MEDIUM, false) == 4);
    }

    SECTION("EASY skill → all 5 columns")
    {
        REQUIRE(InstrumentMapper::getDrumColumn((uint)Drums::EASY_KICK, SkillLevel::EASY, false) == 0);
        REQUIRE(InstrumentMapper::getDrumColumn((uint)Drums::EASY_RED, SkillLevel::EASY, false) == 1);
        REQUIRE(InstrumentMapper::getDrumColumn((uint)Drums::EASY_YELLOW, SkillLevel::EASY, false) == 2);
        REQUIRE(InstrumentMapper::getDrumColumn((uint)Drums::EASY_BLUE, SkillLevel::EASY, false) == 3);
        REQUIRE(InstrumentMapper::getDrumColumn((uint)Drums::EASY_GREEN, SkillLevel::EASY, false) == 4);
    }

    SECTION("EXPERT_KICK_2X enabled → column 6")
    {
        REQUIRE(InstrumentMapper::getDrumColumn((uint)Drums::EXPERT_KICK_2X, SkillLevel::EXPERT, true) == 6);
    }

    SECTION("EXPERT_KICK_2X disabled → INVALID_COLUMN")
    {
        REQUIRE(InstrumentMapper::getDrumColumn((uint)Drums::EXPERT_KICK_2X, SkillLevel::EXPERT, false) == uint(-1));
    }

    SECTION("wrong skill for pitch → INVALID_COLUMN")
    {
        REQUIRE(InstrumentMapper::getDrumColumn((uint)Drums::EXPERT_KICK, SkillLevel::EASY, false) == uint(-1));
        REQUIRE(InstrumentMapper::getDrumColumn((uint)Drums::EASY_RED, SkillLevel::EXPERT, false) == uint(-1));
    }
}

// ============================================================================
// getGuitarPitchesForSkill

TEST_CASE("InstrumentMapper - getGuitarPitchesForSkill", "[instrument_mapper][guitar]")
{
    SECTION("each skill returns 6 pitches")
    {
        REQUIRE(InstrumentMapper::getGuitarPitchesForSkill(SkillLevel::EASY).size() == 6);
        REQUIRE(InstrumentMapper::getGuitarPitchesForSkill(SkillLevel::MEDIUM).size() == 6);
        REQUIRE(InstrumentMapper::getGuitarPitchesForSkill(SkillLevel::HARD).size() == 6);
        REQUIRE(InstrumentMapper::getGuitarPitchesForSkill(SkillLevel::EXPERT).size() == 6);
    }

    SECTION("EXPERT contains expected pitches")
    {
        auto pitches = InstrumentMapper::getGuitarPitchesForSkill(SkillLevel::EXPERT);
        REQUIRE(pitches[0] == (uint)Guitar::EXPERT_OPEN);
        REQUIRE(pitches[1] == (uint)Guitar::EXPERT_GREEN);
        REQUIRE(pitches[5] == (uint)Guitar::EXPERT_ORANGE);
    }

    SECTION("EASY contains expected pitches")
    {
        auto pitches = InstrumentMapper::getGuitarPitchesForSkill(SkillLevel::EASY);
        REQUIRE(pitches[0] == (uint)Guitar::EASY_OPEN);
        REQUIRE(pitches[1] == (uint)Guitar::EASY_GREEN);
    }
}

// ============================================================================
// getDrumPitchesForSkill

TEST_CASE("InstrumentMapper - getDrumPitchesForSkill", "[instrument_mapper][drums]")
{
    SECTION("non-EXPERT returns 5 pitches")
    {
        REQUIRE(InstrumentMapper::getDrumPitchesForSkill(SkillLevel::EASY).size() == 5);
        REQUIRE(InstrumentMapper::getDrumPitchesForSkill(SkillLevel::MEDIUM).size() == 5);
        REQUIRE(InstrumentMapper::getDrumPitchesForSkill(SkillLevel::HARD).size() == 5);
    }

    SECTION("EXPERT returns 6 pitches (includes KICK_2X)")
    {
        REQUIRE(InstrumentMapper::getDrumPitchesForSkill(SkillLevel::EXPERT).size() == 6);
    }

    SECTION("EXPERT contains expected pitches")
    {
        auto pitches = InstrumentMapper::getDrumPitchesForSkill(SkillLevel::EXPERT);
        REQUIRE(pitches[0] == (uint)Drums::EXPERT_KICK);
        REQUIRE(pitches[5] == (uint)Drums::EXPERT_KICK_2X);
    }
}

// ============================================================================
// isDrumKick

TEST_CASE("InstrumentMapper - isDrumKick", "[instrument_mapper][drums]")
{
    SECTION("all kick pitches return true")
    {
        REQUIRE(InstrumentMapper::isDrumKick((uint)Drums::EASY_KICK) == true);
        REQUIRE(InstrumentMapper::isDrumKick((uint)Drums::MEDIUM_KICK) == true);
        REQUIRE(InstrumentMapper::isDrumKick((uint)Drums::HARD_KICK) == true);
        REQUIRE(InstrumentMapper::isDrumKick((uint)Drums::EXPERT_KICK) == true);
        REQUIRE(InstrumentMapper::isDrumKick((uint)Drums::EXPERT_KICK_2X) == true);
    }

    SECTION("non-kick pitches return false")
    {
        REQUIRE(InstrumentMapper::isDrumKick((uint)Drums::EXPERT_RED) == false);
        REQUIRE(InstrumentMapper::isDrumKick((uint)Drums::EXPERT_YELLOW) == false);
        REQUIRE(InstrumentMapper::isDrumKick((uint)Drums::EXPERT_BLUE) == false);
        REQUIRE(InstrumentMapper::isDrumKick((uint)Drums::EXPERT_GREEN) == false);
    }
}

// ============================================================================
// getGuitarModifierPitchesForSkill

TEST_CASE("InstrumentMapper - getGuitarModifierPitchesForSkill", "[instrument_mapper][guitar]")
{
    SECTION("each skill returns 6 modifier pitches")
    {
        REQUIRE(InstrumentMapper::getGuitarModifierPitchesForSkill(SkillLevel::EASY).size() == 6);
        REQUIRE(InstrumentMapper::getGuitarModifierPitchesForSkill(SkillLevel::MEDIUM).size() == 6);
        REQUIRE(InstrumentMapper::getGuitarModifierPitchesForSkill(SkillLevel::HARD).size() == 6);
        REQUIRE(InstrumentMapper::getGuitarModifierPitchesForSkill(SkillLevel::EXPERT).size() == 6);
    }

    SECTION("EXPERT contains skill-specific HOPO/STRUM + shared TAP/SP/LANE")
    {
        auto mods = InstrumentMapper::getGuitarModifierPitchesForSkill(SkillLevel::EXPERT);
        REQUIRE(mods[0] == (uint)Guitar::EXPERT_HOPO);
        REQUIRE(mods[1] == (uint)Guitar::EXPERT_STRUM);
        REQUIRE(mods[2] == (uint)Guitar::TAP);
        REQUIRE(mods[3] == (uint)Guitar::SP);
    }

    SECTION("EASY contains EASY_HOPO/EASY_STRUM")
    {
        auto mods = InstrumentMapper::getGuitarModifierPitchesForSkill(SkillLevel::EASY);
        REQUIRE(mods[0] == (uint)Guitar::EASY_HOPO);
        REQUIRE(mods[1] == (uint)Guitar::EASY_STRUM);
    }
}

// ============================================================================
// getDrumModifierPitches

TEST_CASE("InstrumentMapper - getDrumModifierPitches", "[instrument_mapper][drums]")
{
    SECTION("returns 6 modifier pitches")
    {
        REQUIRE(InstrumentMapper::getDrumModifierPitches().size() == 6);
    }

    SECTION("contains TOM markers, SP, and lanes")
    {
        auto mods = InstrumentMapper::getDrumModifierPitches();
        REQUIRE(mods[0] == (uint)Drums::TOM_YELLOW);
        REQUIRE(mods[1] == (uint)Drums::TOM_BLUE);
        REQUIRE(mods[2] == (uint)Drums::TOM_GREEN);
        REQUIRE(mods[3] == (uint)Drums::SP);
        REQUIRE(mods[4] == (uint)Drums::LANE_1);
        REQUIRE(mods[5] == (uint)Drums::LANE_2);
    }
}

// ============================================================================
// isModifier

TEST_CASE("InstrumentMapper - isModifier", "[instrument_mapper]")
{
    SECTION("guitar modifiers return true")
    {
        REQUIRE(InstrumentMapper::isModifier((uint)Guitar::EXPERT_HOPO) == true);
        REQUIRE(InstrumentMapper::isModifier((uint)Guitar::EXPERT_STRUM) == true);
        REQUIRE(InstrumentMapper::isModifier((uint)Guitar::HARD_HOPO) == true);
        REQUIRE(InstrumentMapper::isModifier((uint)Guitar::HARD_STRUM) == true);
        REQUIRE(InstrumentMapper::isModifier((uint)Guitar::MEDIUM_HOPO) == true);
        REQUIRE(InstrumentMapper::isModifier((uint)Guitar::MEDIUM_STRUM) == true);
        REQUIRE(InstrumentMapper::isModifier((uint)Guitar::EASY_HOPO) == true);
        REQUIRE(InstrumentMapper::isModifier((uint)Guitar::EASY_STRUM) == true);
        REQUIRE(InstrumentMapper::isModifier((uint)Guitar::TAP) == true);
        REQUIRE(InstrumentMapper::isModifier((uint)Guitar::SP) == true);
        REQUIRE(InstrumentMapper::isModifier((uint)Guitar::LANE_1) == true);
        REQUIRE(InstrumentMapper::isModifier((uint)Guitar::LANE_2) == true);
    }

    SECTION("drum modifiers return true")
    {
        REQUIRE(InstrumentMapper::isModifier((uint)Drums::TOM_YELLOW) == true);
        REQUIRE(InstrumentMapper::isModifier((uint)Drums::TOM_BLUE) == true);
        REQUIRE(InstrumentMapper::isModifier((uint)Drums::TOM_GREEN) == true);
        REQUIRE(InstrumentMapper::isModifier((uint)Drums::SP) == true);
        REQUIRE(InstrumentMapper::isModifier((uint)Drums::LANE_1) == true);
        REQUIRE(InstrumentMapper::isModifier((uint)Drums::LANE_2) == true);
    }

    SECTION("playable pitches return false")
    {
        REQUIRE(InstrumentMapper::isModifier((uint)Guitar::EXPERT_GREEN) == false);
        REQUIRE(InstrumentMapper::isModifier((uint)Guitar::EXPERT_RED) == false);
        REQUIRE(InstrumentMapper::isModifier((uint)Drums::EXPERT_KICK) == false);
        REQUIRE(InstrumentMapper::isModifier((uint)Drums::EXPERT_RED) == false);
    }
}

// ============================================================================
// columnToEliteDrumPitch (write-mode inverse of getEliteDrumColumn)

TEST_CASE("InstrumentMapper - columnToEliteDrumPitch", "[instrument_mapper][elite]")
{
    SECTION("EXPERT columns map to the Expert octave")
    {
        REQUIRE(InstrumentMapper::columnToEliteDrumPitch(SkillLevel::EXPERT, 0) == (int)EliteDrums::EXPERT_KICK);
        REQUIRE(InstrumentMapper::columnToEliteDrumPitch(SkillLevel::EXPERT, 1) == (int)EliteDrums::EXPERT_SNARE);
        REQUIRE(InstrumentMapper::columnToEliteDrumPitch(SkillLevel::EXPERT, 2) == (int)EliteDrums::EXPERT_HIHAT);
        REQUIRE(InstrumentMapper::columnToEliteDrumPitch(SkillLevel::EXPERT, 3) == (int)EliteDrums::EXPERT_LCRASH);
        REQUIRE(InstrumentMapper::columnToEliteDrumPitch(SkillLevel::EXPERT, 4) == (int)EliteDrums::EXPERT_TOM1);
        REQUIRE(InstrumentMapper::columnToEliteDrumPitch(SkillLevel::EXPERT, 5) == (int)EliteDrums::EXPERT_TOM2);
        REQUIRE(InstrumentMapper::columnToEliteDrumPitch(SkillLevel::EXPERT, 6) == (int)EliteDrums::EXPERT_TOM3);
        REQUIRE(InstrumentMapper::columnToEliteDrumPitch(SkillLevel::EXPERT, 7) == (int)EliteDrums::EXPERT_RIDE);
        REQUIRE(InstrumentMapper::columnToEliteDrumPitch(SkillLevel::EXPERT, 8) == (int)EliteDrums::EXPERT_RCRASH);
        REQUIRE(InstrumentMapper::columnToEliteDrumPitch(SkillLevel::EXPERT, ELITE_KICK_2X_COLUMN)
                == (int)EliteDrums::EXPERT_KICK_2X);
    }

    SECTION("lower difficulties sit a fixed -24 per step")
    {
        REQUIRE(InstrumentMapper::columnToEliteDrumPitch(SkillLevel::HARD,   0) == (int)EliteDrums::HARD_KICK);
        REQUIRE(InstrumentMapper::columnToEliteDrumPitch(SkillLevel::MEDIUM, 0) == (int)EliteDrums::MEDIUM_KICK);
        REQUIRE(InstrumentMapper::columnToEliteDrumPitch(SkillLevel::EASY,   0) == (int)EliteDrums::EASY_KICK);
        REQUIRE(InstrumentMapper::columnToEliteDrumPitch(SkillLevel::HARD,   8) == (int)EliteDrums::HARD_RCRASH);
        REQUIRE(InstrumentMapper::columnToEliteDrumPitch(SkillLevel::EASY,   8) == (int)EliteDrums::EASY_RCRASH);
        REQUIRE(InstrumentMapper::columnToEliteDrumPitch(SkillLevel::EASY, ELITE_KICK_2X_COLUMN)
                == (int)EliteDrums::EASY_KICK_2X);
    }

    SECTION("round-trips through getEliteDrumColumn for every skill and column")
    {
        for (auto skill : {SkillLevel::EASY, SkillLevel::MEDIUM, SkillLevel::HARD, SkillLevel::EXPERT})
            for (int col = 0; col <= ELITE_KICK_2X_COLUMN; ++col)
            {
                int pitch = InstrumentMapper::columnToEliteDrumPitch(skill, col);
                REQUIRE(pitch >= 0);
                REQUIRE(InstrumentMapper::getEliteDrumColumn((uint)pitch, skill, true) == (uint)col);
            }
    }

    SECTION("lane 6 is Tom 3, not a 2x kick")
    {
        // 4-lane puts the 2x kick on column 6, so a part-blind write path turns an elite
        // Tom 3 click into a kick. Pin the elite meaning.
        REQUIRE(InstrumentMapper::columnToEliteDrumPitch(SkillLevel::EXPERT, 6) == (int)EliteDrums::EXPERT_TOM3);
        REQUIRE(isDrumKick(6, Part::ELITE_DRUMS) == false);
        REQUIRE(isDrumKick(ELITE_KICK_2X_COLUMN, Part::ELITE_DRUMS) == true);
    }

    SECTION("out-of-range columns return -1")
    {
        REQUIRE(InstrumentMapper::columnToEliteDrumPitch(SkillLevel::EXPERT, -1) == -1);
        REQUIRE(InstrumentMapper::columnToEliteDrumPitch(SkillLevel::EXPERT, 10) == -1);
    }
}

TEST_CASE("InstrumentMapper - elite kick conflicts", "[instrument_mapper][elite]")
{
    SECTION("only the kick pair counts as a kick")
    {
        REQUIRE(InstrumentMapper::isEliteDrumKick((uint)EliteDrums::EXPERT_KICK,    SkillLevel::EXPERT));
        REQUIRE(InstrumentMapper::isEliteDrumKick((uint)EliteDrums::EXPERT_KICK_2X, SkillLevel::EXPERT));
        REQUIRE(InstrumentMapper::isEliteDrumKick((uint)EliteDrums::EXPERT_TOM3,    SkillLevel::EXPERT) == false);
        REQUIRE(InstrumentMapper::isEliteDrumKick((uint)EliteDrums::EXPERT_SNARE,   SkillLevel::EXPERT) == false);
    }

    SECTION("a kick at the wrong difficulty is not this skill's kick")
    {
        REQUIRE(InstrumentMapper::isEliteDrumKick((uint)EliteDrums::EXPERT_KICK, SkillLevel::HARD) == false);
        REQUIRE(InstrumentMapper::isEliteDrumKick((uint)EliteDrums::HARD_KICK,   SkillLevel::HARD));
    }

    SECTION("each kick names the other as its conflict")
    {
        auto a = InstrumentMapper::getConflictingEliteKick((int)EliteDrums::EXPERT_KICK, SkillLevel::EXPERT);
        REQUIRE(a.pitch == (int)EliteDrums::EXPERT_KICK_2X);
        REQUIRE(a.lane  == ELITE_KICK_2X_COLUMN);

        auto b = InstrumentMapper::getConflictingEliteKick((int)EliteDrums::EXPERT_KICK_2X, SkillLevel::EXPERT);
        REQUIRE(b.pitch == (int)EliteDrums::EXPERT_KICK);
        REQUIRE(b.lane  == DRUM_KICK_COLUMN);

        auto c = InstrumentMapper::getConflictingEliteKick((int)EliteDrums::EASY_KICK_2X, SkillLevel::EASY);
        REQUIRE(c.pitch == (int)EliteDrums::EASY_KICK);
        REQUIRE(c.lane  == DRUM_KICK_COLUMN);
    }
}

// ============================================================================
// getEliteRollLaneColumn / isEliteRollLane (elite roll/tremolo lanes 110..118)

TEST_CASE("InstrumentMapper - getEliteRollLaneColumn", "[instrument_mapper][elite]")
{
    SECTION("roll pitches 110..118 map linearly to columns 0..8")
    {
        REQUIRE(InstrumentMapper::getEliteRollLaneColumn((uint)EliteDrums::ROLL_KICK)   == 0);
        REQUIRE(InstrumentMapper::getEliteRollLaneColumn((uint)EliteDrums::ROLL_SNARE)  == 1);
        REQUIRE(InstrumentMapper::getEliteRollLaneColumn((uint)EliteDrums::ROLL_HIHAT)  == 2);
        REQUIRE(InstrumentMapper::getEliteRollLaneColumn((uint)EliteDrums::ROLL_LCRASH) == 3);
        REQUIRE(InstrumentMapper::getEliteRollLaneColumn((uint)EliteDrums::ROLL_TOM1)   == 4);
        REQUIRE(InstrumentMapper::getEliteRollLaneColumn((uint)EliteDrums::ROLL_TOM2)   == 5);
        REQUIRE(InstrumentMapper::getEliteRollLaneColumn((uint)EliteDrums::ROLL_TOM3)   == 6);
        REQUIRE(InstrumentMapper::getEliteRollLaneColumn((uint)EliteDrums::ROLL_RIDE)   == 7);
        REQUIRE(InstrumentMapper::getEliteRollLaneColumn((uint)EliteDrums::ROLL_RCRASH) == 8);
    }

    SECTION("non-roll pitches return INVALID_COLUMN")
    {
        REQUIRE(InstrumentMapper::getEliteRollLaneColumn(109) == uint(-1));   // unused (no 2x-kick roll)
        REQUIRE(InstrumentMapper::getEliteRollLaneColumn((uint)EliteDrums::ROLL_STOMP) == uint(-1)); // 108, no column yet
        REQUIRE(InstrumentMapper::getEliteRollLaneColumn((uint)EliteDrums::EXPERT_SNARE) == uint(-1));
        REQUIRE(InstrumentMapper::getEliteRollLaneColumn((uint)EliteDrums::SP) == uint(-1));
    }

    SECTION("isEliteRollLane matches the mapping")
    {
        REQUIRE(InstrumentMapper::isEliteRollLane((uint)EliteDrums::ROLL_KICK)   == true);
        REQUIRE(InstrumentMapper::isEliteRollLane((uint)EliteDrums::ROLL_RCRASH) == true);
        REQUIRE(InstrumentMapper::isEliteRollLane(109) == false);
        REQUIRE(InstrumentMapper::isEliteRollLane((uint)EliteDrums::SP) == false);
    }

    SECTION("elite isModifier accepts SP and roll lanes, not notes")
    {
        REQUIRE(InstrumentMapper::isModifier((uint)EliteDrums::SP, true) == true);
        REQUIRE(InstrumentMapper::isModifier((uint)EliteDrums::ROLL_TOM3, true) == true);
        REQUIRE(InstrumentMapper::isModifier((uint)EliteDrums::EXPERT_LCRASH, true) == false); // 77 = elite note, not modifier
    }
}
