#include "test_helpers.h"
#include "Editor/AuthoringConfig.h"

using EliteDrums = MidiPitchDefinitions::EliteDrums;
using Drums = MidiPitchDefinitions::Drums;
using Guitar = MidiPitchDefinitions::Guitar;

TEST_CASE("AuthoringConfig - lookup", "[authoring_config]")
{
    SECTION("the three authorable render types resolve")
    {
        REQUIRE(getAuthoringConfig(RenderType::FIVE_FRET) != nullptr);
        REQUIRE(getAuthoringConfig(RenderType::FOUR_LANE_DRUMS) != nullptr);
        REQUIRE(getAuthoringConfig(RenderType::ELITE_DRUMS) != nullptr);
    }

    SECTION("unauthorable render types return null")
    {
        REQUIRE(getAuthoringConfig(RenderType::VOCALS) == nullptr);
        REQUIRE(getAuthoringConfig(RenderType::PRO_KEYS) == nullptr);
    }

    SECTION("the Part overload agrees with the RenderType one")
    {
        REQUIRE(getAuthoringConfig(Part::ELITE_DRUMS) == getAuthoringConfig(RenderType::ELITE_DRUMS));
        REQUIRE(getAuthoringConfig(Part::DRUMS) == getAuthoringConfig(RenderType::FOUR_LANE_DRUMS));
    }
}

TEST_CASE("AuthoringConfig - lane extents", "[authoring_config]")
{
    // 4-lane's 2x kick sits INSIDE the lane range (6) while elite's sits ABOVE its hand
    // lanes (9). A single shared expression can't produce both, which is what clamped
    // elite to 6 and hid Ride and R-Crash.
    SECTION("4-lane drums")
    {
        const auto* c = getAuthoringConfig(RenderType::FOUR_LANE_DRUMS);
        REQUIRE(c->highestLane == 4);
        REQUIRE(c->highestLaneKick2x == DRUM_KICK_2X_COLUMN);
        REQUIRE(c->kick2xColumn == DRUM_KICK_2X_COLUMN);
    }

    SECTION("elite drums reaches Ride and R-Crash")
    {
        const auto* c = getAuthoringConfig(RenderType::ELITE_DRUMS);
        REQUIRE(c->highestLane == 8);
        REQUIRE(c->highestLaneKick2x == ELITE_KICK_2X_COLUMN);
        REQUIRE(c->kick2xColumn == ELITE_KICK_2X_COLUMN);
    }

    SECTION("guitar has no kick")
    {
        const auto* c = getAuthoringConfig(RenderType::FIVE_FRET);
        REQUIRE(c->highestLane == 5);
        REQUIRE(c->kick2xColumn == -1);
    }
}

TEST_CASE("AuthoringConfig - laneToPitch", "[authoring_config]")
{
    SECTION("elite lane 6 is Tom 3, not a kick")
    {
        const auto* c = getAuthoringConfig(RenderType::ELITE_DRUMS);
        REQUIRE(c->laneToPitch(SkillLevel::EXPERT, 6, true) == (int)EliteDrums::EXPERT_TOM3);
        REQUIRE(c->laneToPitch(SkillLevel::EXPERT, 0, true) == (int)EliteDrums::EXPERT_KICK);
        REQUIRE(c->laneToPitch(SkillLevel::EXPERT, ELITE_KICK_2X_COLUMN, true)
                == (int)EliteDrums::EXPERT_KICK_2X);
    }

    SECTION("elite reaches every hand lane")
    {
        const auto* c = getAuthoringConfig(RenderType::ELITE_DRUMS);
        for (int lane = 0; lane <= 8; ++lane)
            REQUIRE(c->laneToPitch(SkillLevel::EXPERT, lane, true) >= 0);
    }

    SECTION("elite 2x kick is refused when the toggle is off")
    {
        const auto* c = getAuthoringConfig(RenderType::ELITE_DRUMS);
        REQUIRE(c->laneToPitch(SkillLevel::EXPERT, ELITE_KICK_2X_COLUMN, false) == -1);
    }

    SECTION("4-lane column 6 IS the 2x kick")
    {
        const auto* c = getAuthoringConfig(RenderType::FOUR_LANE_DRUMS);
        REQUIRE(c->laneToPitch(SkillLevel::EXPERT, DRUM_KICK_2X_COLUMN, true)
                == (int)Drums::EXPERT_KICK_2X);
        REQUIRE(c->laneToPitch(SkillLevel::EXPERT, DRUM_KICK_2X_COLUMN, false) == -1);
        REQUIRE(c->laneToPitch(SkillLevel::EXPERT, 1, false) == (int)Drums::EXPERT_RED);
    }

    SECTION("guitar lane 0 is the open bar")
    {
        const auto* c = getAuthoringConfig(RenderType::FIVE_FRET);
        REQUIRE(c->laneToPitch(SkillLevel::EXPERT, 0, false) == (int)Guitar::EXPERT_OPEN);
        REQUIRE(c->laneToPitch(SkillLevel::EXPERT, 5, false) == (int)Guitar::EXPERT_ORANGE);
    }
}

TEST_CASE("AuthoringConfig - kick conflicts", "[authoring_config]")
{
    SECTION("elite pairs its own kicks and ignores toms")
    {
        const auto* c = getAuthoringConfig(RenderType::ELITE_DRUMS);
        REQUIRE(c->isKickPitch((uint)EliteDrums::EXPERT_KICK, SkillLevel::EXPERT));
        REQUIRE(c->isKickPitch((uint)EliteDrums::EXPERT_TOM3, SkillLevel::EXPERT) == false);

        auto conflict = c->conflictingKick((int)EliteDrums::EXPERT_KICK, SkillLevel::EXPERT);
        REQUIRE(conflict.pitch == (int)EliteDrums::EXPERT_KICK_2X);
        REQUIRE(conflict.lane == ELITE_KICK_2X_COLUMN);
    }

    SECTION("guitar has no kick to conflict with")
    {
        const auto* c = getAuthoringConfig(RenderType::FIVE_FRET);
        REQUIRE(c->isKickPitch((uint)Guitar::EXPERT_GREEN, SkillLevel::EXPERT) == false);
        REQUIRE(c->conflictingKick((int)Guitar::EXPERT_GREEN, SkillLevel::EXPERT).lane == -1);
    }
}

TEST_CASE("AuthoringConfig - cymbal toggle", "[authoring_config]")
{
    // Elite fixes cymbal-ness by lane, so its toggle is inert and the sub-toolbar omits it.
    REQUIRE(getAuthoringConfig(RenderType::FOUR_LANE_DRUMS)->hasCymbalToggle);
    REQUIRE(getAuthoringConfig(RenderType::ELITE_DRUMS)->hasCymbalToggle == false);
    REQUIRE(getAuthoringConfig(RenderType::FIVE_FRET)->hasCymbalToggle == false);
}
