#include "test_helpers.h"
#include "Midi/Processing/MidiInterpreter.h"

using Guitar = MidiPitchDefinitions::Guitar;
using Drums = MidiPitchDefinitions::Drums;

// Helper: resolve and extract EXPERT difficulty
static DifficultyWindow resolveExpert(TestFixture& f, PPQ start, PPQ end, PPQ latency = PPQ(0.0))
{
    MidiInterpreter interp(f.state, f.array, f.lock);
    PartWindow pw = interp.resolveAllDifficulties(start, end, latency);
    return pw.forSkill(SkillLevel::EXPERT);
}

// ============================================================================
// Basic note rendering

TEST_CASE("resolveAllDifficulties - basic note rendering", "[resolve][track_window]")
{
    TestFixture f;

    SECTION("single expert green at PPQ 2.0 → column 1 populated")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0));

        auto dw = resolveExpert(f, PPQ(0.0), PPQ(4.0));
        REQUIRE(dw.trackWindow.count(PPQ(2.0)) == 1);
        REQUIRE(dw.trackWindow[PPQ(2.0)][1].gem == Gem::NOTE);
        REQUIRE(dw.trackWindow[PPQ(2.0)][0].gem == Gem::NONE);
    }

    SECTION("note outside window → not in output")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(5.0), PPQ(6.0));

        auto dw = resolveExpert(f, PPQ(0.0), PPQ(4.0));
        REQUIRE(dw.trackWindow.count(PPQ(5.0)) == 0);
    }

    SECTION("guitar chord → both columns populated in same frame")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0));
        f.addNote((uint)Guitar::EXPERT_RED, PPQ(2.0), PPQ(3.0));

        auto dw = resolveExpert(f, PPQ(0.0), PPQ(4.0));
        REQUIRE(dw.trackWindow.count(PPQ(2.0)) == 1);
        REQUIRE(dw.trackWindow[PPQ(2.0)][1].gem == Gem::NOTE); // GREEN
        REQUIRE(dw.trackWindow[PPQ(2.0)][2].gem == Gem::NOTE); // RED
    }
}

// ============================================================================
// Star power propagation

TEST_CASE("resolveAllDifficulties - star power", "[resolve][star_power]")
{
    TestFixture f;

    SECTION("SP modifier held across note → starPower = true")
    {
        f.addModifier((uint)Guitar::SP, PPQ(1.0), PPQ(5.0));
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0));

        auto dw = resolveExpert(f, PPQ(0.0), PPQ(6.0));
        REQUIRE(dw.trackWindow[PPQ(2.0)][1].starPower == true);
    }

    SECTION("SP modifier NOT held → starPower = false")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0));

        auto dw = resolveExpert(f, PPQ(0.0), PPQ(4.0));
        REQUIRE(dw.trackWindow[PPQ(2.0)][1].starPower == false);
    }

    SECTION("elite drums: SP modifier (104) held across snare → starPower = true")
    {
        using ED = MidiPitchDefinitions::EliteDrums;
        f.state.setProperty("part", (int)Part::ELITE_DRUMS, nullptr);
        f.addNote((uint)ED::EXPERT_SNARE, PPQ(2.0), PPQ(2.5));
        f.addModifier((uint)ED::SP, PPQ(1.0), PPQ(5.0));

        auto dw = resolveExpert(f, PPQ(0.0), PPQ(6.0));
        uint col = InstrumentMapper::getEliteDrumColumn((uint)ED::EXPERT_SNARE, SkillLevel::EXPERT, true);
        REQUIRE(dw.trackWindow[PPQ(2.0)][col].gem == Gem::NOTE);
        REQUIRE(dw.trackWindow[PPQ(2.0)][col].starPower == true);
    }
}

// ============================================================================
// Modifier pitches not rendered as notes

TEST_CASE("resolveAllDifficulties - modifier pitches not rendered", "[resolve]")
{
    TestFixture f;

    SECTION("HOPO/STRUM/TAP/SP/LANE modifier pitches → NOT rendered as glyphs")
    {
        f.addModifier((uint)Guitar::EXPERT_HOPO, PPQ(1.0), PPQ(5.0));
        f.addModifier((uint)Guitar::EXPERT_STRUM, PPQ(1.0), PPQ(5.0));
        f.addModifier((uint)Guitar::TAP, PPQ(1.0), PPQ(5.0));
        f.addModifier((uint)Guitar::SP, PPQ(1.0), PPQ(5.0));
        f.addModifier((uint)Guitar::LANE_1, PPQ(1.0), PPQ(5.0));

        auto dw = resolveExpert(f, PPQ(0.0), PPQ(6.0));

        for (auto& [ppq, frame] : dw.trackWindow)
        {
            for (uint col = 0; col < LANE_COUNT; col++)
            {
                REQUIRE(frame[col].gem == Gem::NONE);
            }
        }
    }
}

// ============================================================================
// Drum mode

TEST_CASE("resolveAllDifficulties - drum mode", "[resolve][drums]")
{
    TestFixture f;
    f.state.setProperty("part", (int)Part::DRUMS, nullptr);

    SECTION("expert kick → column 0")
    {
        f.addNote((uint)Drums::EXPERT_KICK, PPQ(2.0), PPQ(2.5));

        auto dw = resolveExpert(f, PPQ(0.0), PPQ(4.0));
        REQUIRE(dw.trackWindow[PPQ(2.0)][0].gem == Gem::NOTE);
    }

    SECTION("kick2x enabled, pitch 95 → column 6")
    {
        f.addNote((uint)Drums::EXPERT_KICK_2X, PPQ(2.0), PPQ(2.5));

        auto dw = resolveExpert(f, PPQ(0.0), PPQ(4.0));
        REQUIRE(dw.trackWindow[PPQ(2.0)][6].gem == Gem::NOTE);
    }

    SECTION("pro drums: yellow without TOM → CYM")
    {
        f.state.setProperty("drumType", (int)DrumType::PRO, nullptr);
        f.addNote((uint)Drums::EXPERT_YELLOW, PPQ(2.0), PPQ(2.5));

        auto dw = resolveExpert(f, PPQ(0.0), PPQ(4.0));
        REQUIRE(dw.trackWindow[PPQ(2.0)][2].gem == Gem::CYM);
    }

    SECTION("pro drums: yellow with TOM_YELLOW → NOTE")
    {
        f.state.setProperty("drumType", (int)DrumType::PRO, nullptr);
        f.addModifier((uint)Drums::TOM_YELLOW, PPQ(0.0), PPQ(4.0));
        f.addNote((uint)Drums::EXPERT_YELLOW, PPQ(2.0), PPQ(2.5));

        auto dw = resolveExpert(f, PPQ(0.0), PPQ(4.0));
        REQUIRE(dw.trackWindow[PPQ(2.0)][2].gem == Gem::NOTE);
    }

    SECTION("dynamics: ghost velocity on red → HOPO_GHOST")
    {
        f.addNote((uint)Drums::EXPERT_RED, PPQ(2.0), PPQ(2.5), (uint8_t)Dynamic::GHOST);

        auto dw = resolveExpert(f, PPQ(0.0), PPQ(4.0));
        REQUIRE(dw.trackWindow[PPQ(2.0)][1].gem == Gem::HOPO_GHOST);
    }
}

// ============================================================================
// Elite drums hi-hat pedal state (Open / Closed / Indifferent)

TEST_CASE("resolveAllDifficulties - elite hi-hat pedal state", "[resolve][elite][hihat]")
{
    using ED = MidiPitchDefinitions::EliteDrums;
    TestFixture f;
    f.state.setProperty("part", (int)Part::ELITE_DRUMS, nullptr);
    const uint HH = (uint)ELITE_HIHAT_COLUMN;

    SECTION("bare yellow hi-hat → Open (spec default)")
    {
        f.addNote((uint)ED::EXPERT_HIHAT, PPQ(2.0), PPQ(2.5));

        auto dw = resolveExpert(f, PPQ(0.0), PPQ(4.0));
        REQUIRE(dw.trackWindow[PPQ(2.0)][HH].gem == Gem::CYM);
        REQUIRE(dw.trackWindow[PPQ(2.0)][HH].hihat == HiHatState::Open);
    }

    SECTION("coincident Pedal Down → Closed")
    {
        f.addNote((uint)ED::EXPERT_HIHAT, PPQ(2.0), PPQ(2.5));
        f.addModifier((uint)ED::EXPERT_PEDAL, PPQ(2.0), PPQ(2.5));

        auto dw = resolveExpert(f, PPQ(0.0), PPQ(4.0));
        REQUIRE(dw.trackWindow[PPQ(2.0)][HH].hihat == HiHatState::Closed);
    }

    SECTION("Pedal Down dragged under the note → Closed")
    {
        f.addNote((uint)ED::EXPERT_HIHAT, PPQ(2.0), PPQ(2.5));
        f.addModifier((uint)ED::EXPERT_PEDAL, PPQ(1.0), PPQ(3.0));   // pedal held across the note

        auto dw = resolveExpert(f, PPQ(0.0), PPQ(4.0));
        REQUIRE(dw.trackWindow[PPQ(2.0)][HH].hihat == HiHatState::Closed);
    }

    SECTION("coincident Indifferent marker → Indifferent")
    {
        f.addNote((uint)ED::EXPERT_HIHAT, PPQ(2.0), PPQ(2.5));
        f.addModifier((uint)ED::EXPERT_INDIFFERENT, PPQ(2.0), PPQ(2.5));

        auto dw = resolveExpert(f, PPQ(0.0), PPQ(4.0));
        REQUIRE(dw.trackWindow[PPQ(2.0)][HH].hihat == HiHatState::Indifferent);
    }

    SECTION("Indifferent + Pedal Down together → Indifferent wins (Appendix B)")
    {
        f.addNote((uint)ED::EXPERT_HIHAT, PPQ(2.0), PPQ(2.5));
        f.addModifier((uint)ED::EXPERT_PEDAL, PPQ(2.0), PPQ(2.5));
        f.addModifier((uint)ED::EXPERT_INDIFFERENT, PPQ(2.0), PPQ(2.5));

        auto dw = resolveExpert(f, PPQ(0.0), PPQ(4.0));
        REQUIRE(dw.trackWindow[PPQ(2.0)][HH].hihat == HiHatState::Indifferent);
    }

    SECTION("pedal state applies to the hi-hat lane only, not other cymbals")
    {
        f.addNote((uint)ED::EXPERT_LCRASH, PPQ(2.0), PPQ(2.5));   // col 3, purple cymbal
        f.addModifier((uint)ED::EXPERT_PEDAL, PPQ(2.0), PPQ(2.5));

        auto dw = resolveExpert(f, PPQ(0.0), PPQ(4.0));
        REQUIRE(dw.trackWindow[PPQ(2.0)][3].hihat == HiHatState::None);
    }

    SECTION("a lower-difficulty pedal does not close an Expert hi-hat")
    {
        f.addNote((uint)ED::EXPERT_HIHAT, PPQ(2.0), PPQ(2.5));
        f.addModifier((uint)ED::HARD_PEDAL, PPQ(2.0), PPQ(2.5));   // Hard pedal, not Expert

        auto dw = resolveExpert(f, PPQ(0.0), PPQ(4.0));
        REQUIRE(dw.trackWindow[PPQ(2.0)][HH].hihat == HiHatState::Open);
    }

    SECTION("Pedal Down / Indifferent modifiers are not themselves rendered as gems")
    {
        f.addModifier((uint)ED::EXPERT_PEDAL, PPQ(2.0), PPQ(2.5));
        f.addModifier((uint)ED::EXPERT_INDIFFERENT, PPQ(3.0), PPQ(3.5));

        auto dw = resolveExpert(f, PPQ(0.0), PPQ(4.0));
        for (auto& [ppq, frame] : dw.trackWindow)
            for (uint col = 0; col < LANE_COUNT; col++)
                REQUIRE(frame[col].gem == Gem::NONE);
    }
}

// ============================================================================
// Guitar sustains

TEST_CASE("resolveAllDifficulties - guitar sustains", "[resolve][sustains]")
{
    TestFixture f;

    SECTION("long note produces sustain with correct column")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.0), PPQ(3.0));

        auto dw = resolveExpert(f, PPQ(0.0), PPQ(4.0), PPQ(5.0));

        bool found = false;
        for (auto& s : dw.sustainWindow)
        {
            if (s.sustainType == SustainType::SUSTAIN && s.gemColumn == 1)
            {
                found = true;
                REQUIRE(s.startPPQ == PPQ(1.0));
            }
        }
        REQUIRE(found);
    }

    SECTION("short note (below MIN_SUSTAIN_LENGTH) → no sustain")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.0), PPQ(1.1));

        auto dw = resolveExpert(f, PPQ(0.0), PPQ(4.0), PPQ(5.0));

        bool foundSustain = false;
        for (auto& s : dw.sustainWindow)
        {
            if (s.sustainType == SustainType::SUSTAIN && s.gemColumn == 1)
                foundSustain = true;
        }
        REQUIRE_FALSE(foundSustain);
    }

    SECTION("SP held at sustain start → starPower = true")
    {
        f.addModifier((uint)Guitar::SP, PPQ(0.5), PPQ(4.0));
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.0), PPQ(3.0));

        auto dw = resolveExpert(f, PPQ(0.0), PPQ(4.0), PPQ(5.0));

        bool found = false;
        for (auto& s : dw.sustainWindow)
        {
            if (s.sustainType == SustainType::SUSTAIN && s.gemColumn == 1)
            {
                found = true;
                REQUIRE(s.gemType.starPower == true);
            }
        }
        REQUIRE(found);
    }
}

// ============================================================================
// Multi-difficulty output

TEST_CASE("resolveAllDifficulties - multi-difficulty", "[resolve][multi_diff]")
{
    TestFixture f;

    SECTION("expert note not visible at easy difficulty")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(2.0), PPQ(3.0));

        MidiInterpreter interp(f.state, f.array, f.lock);
        PartWindow pw = interp.resolveAllDifficulties(PPQ(0.0), PPQ(4.0), PPQ(0.0));

        REQUIRE(pw.forSkill(SkillLevel::EXPERT).trackWindow.count(PPQ(2.0)) == 1);
        REQUIRE(pw.forSkill(SkillLevel::EASY).trackWindow.empty());
    }

    SECTION("easy note visible at easy, not expert")
    {
        f.addNote((uint)Guitar::EASY_GREEN, PPQ(2.0), PPQ(3.0));

        MidiInterpreter interp(f.state, f.array, f.lock);
        PartWindow pw = interp.resolveAllDifficulties(PPQ(0.0), PPQ(4.0), PPQ(0.0));

        REQUIRE(pw.forSkill(SkillLevel::EASY).trackWindow.count(PPQ(2.0)) == 1);
        REQUIRE(pw.forSkill(SkillLevel::EXPERT).trackWindow.empty());
    }
}

// ============================================================================
// Lane sustains

TEST_CASE("resolveAllDifficulties - lane sustains", "[resolve][lanes]")
{
    TestFixture f;

    SECTION("LANE_1 modifier with playable note → SustainType::LANE")
    {
        f.addNote((uint)Guitar::EXPERT_GREEN, PPQ(1.5), PPQ(2.5));
        f.addModifier((uint)Guitar::LANE_1, PPQ(1.0), PPQ(3.0));

        auto dw = resolveExpert(f, PPQ(0.0), PPQ(4.0), PPQ(5.0));

        bool foundLane = false;
        for (auto& s : dw.sustainWindow)
        {
            if (s.sustainType == SustainType::LANE)
            {
                foundLane = true;
                REQUIRE(s.gemColumn == 1); // GREEN column
            }
        }
        REQUIRE(foundLane);
    }
}
