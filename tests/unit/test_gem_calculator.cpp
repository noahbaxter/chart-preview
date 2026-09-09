#include "test_helpers.h"
#include "Midi/Utils/GemCalculator.h"
#include "Editor/AuthoringTypes.h"

// ============================================================================
// resolveGuitarGem — pure function, no locks

TEST_CASE("resolveGuitarGem - modifier priority", "[gem_calculator][guitar]")
{
    SECTION("strum forced → NOTE")
    {
        REQUIRE(GemCalculator::resolveGuitarGem(false, false, false, true, false) == Gem::NOTE);
    }

    SECTION("HOPO forced → HOPO_GHOST")
    {
        REQUIRE(GemCalculator::resolveGuitarGem(false, false, true, false, false) == Gem::HOPO_GHOST);
    }

    SECTION("TAP forced → TAP_ACCENT")
    {
        REQUIRE(GemCalculator::resolveGuitarGem(false, false, false, false, true) == Gem::TAP_ACCENT);
    }

    SECTION("strum + HOPO both forced → NOTE (strum wins)")
    {
        REQUIRE(GemCalculator::resolveGuitarGem(false, false, true, true, false) == Gem::NOTE);
    }

    SECTION("strum + TAP both forced → NOTE (strum wins)")
    {
        REQUIRE(GemCalculator::resolveGuitarGem(false, false, false, true, true) == Gem::NOTE);
    }

    SECTION("no modifiers → NOTE (default)")
    {
        REQUIRE(GemCalculator::resolveGuitarGem(false, false, false, false, false) == Gem::NOTE);
    }
}

TEST_CASE("resolveGuitarGem - chord always strum", "[gem_calculator][guitar][chord]")
{
    SECTION("chord, no modifiers → NOTE")
    {
        REQUIRE(GemCalculator::resolveGuitarGem(true, false, false, false, false) == Gem::NOTE);
    }

    SECTION("chord + autoHOPO → NOTE (chord wins)")
    {
        REQUIRE(GemCalculator::resolveGuitarGem(true, true, false, false, false) == Gem::NOTE);
    }

    SECTION("chord + forced HOPO → HOPO_GHOST (forced wins over chord)")
    {
        REQUIRE(GemCalculator::resolveGuitarGem(true, false, true, false, false) == Gem::HOPO_GHOST);
    }
}

TEST_CASE("resolveGuitarGem - auto-HOPO", "[gem_calculator][guitar][auto_hopo]")
{
    SECTION("autoHOPO true, single note → HOPO_GHOST")
    {
        REQUIRE(GemCalculator::resolveGuitarGem(false, true, false, false, false) == Gem::HOPO_GHOST);
    }

    SECTION("autoHOPO false → NOTE")
    {
        REQUIRE(GemCalculator::resolveGuitarGem(false, false, false, false, false) == Gem::NOTE);
    }
}

// ============================================================================
// resolveDrumGem — pure function, no locks

TEST_CASE("resolveDrumGem - cymbal vs tom", "[gem_calculator][drums]")
{
    SECTION("not cymbal, no dynamics → NOTE")
    {
        REQUIRE(GemCalculator::resolveDrumGem(false, false, Dynamic::NONE) == Gem::NOTE);
    }

    SECTION("cymbal, no dynamics → CYM")
    {
        REQUIRE(GemCalculator::resolveDrumGem(true, false, Dynamic::NONE) == Gem::CYM);
    }

    SECTION("cymbal, dynamics enabled, ghost → CYM_GHOST")
    {
        REQUIRE(GemCalculator::resolveDrumGem(true, true, Dynamic::GHOST) == Gem::CYM_GHOST);
    }

    SECTION("cymbal, dynamics enabled, accent → CYM_ACCENT")
    {
        REQUIRE(GemCalculator::resolveDrumGem(true, true, Dynamic::ACCENT) == Gem::CYM_ACCENT);
    }

    SECTION("not cymbal, dynamics enabled, ghost → HOPO_GHOST")
    {
        REQUIRE(GemCalculator::resolveDrumGem(false, true, Dynamic::GHOST) == Gem::HOPO_GHOST);
    }

    SECTION("not cymbal, dynamics enabled, accent → TAP_ACCENT")
    {
        REQUIRE(GemCalculator::resolveDrumGem(false, true, Dynamic::ACCENT) == Gem::TAP_ACCENT);
    }

    SECTION("cymbal, dynamics disabled, ghost velocity → CYM (dynamics ignored)")
    {
        REQUIRE(GemCalculator::resolveDrumGem(true, false, Dynamic::GHOST) == Gem::CYM);
    }
}

// ============================================================================
// authorsCymbal — which lanes a click should write as a cymbal, per part

TEST_CASE("authorsCymbal - 4-lane drums", "[gem_calculator][drums]")
{
    SECTION("lanes 2-4 follow the Cym toggle")
    {
        for (uint lane : {2u, 3u, 4u})
        {
            REQUIRE(authorsCymbal(lane, Part::DRUMS, true));
            REQUIRE(authorsCymbal(lane, Part::DRUMS, false) == false);
        }
    }

    SECTION("kick and snare are never cymbals")
    {
        REQUIRE(authorsCymbal(0, Part::DRUMS, true) == false);
        REQUIRE(authorsCymbal(1, Part::DRUMS, true) == false);
    }
}

TEST_CASE("authorsCymbal - elite drums", "[gem_calculator][elite]")
{
    // Elite lanes are drum XOR cymbal, fixed by lane, so the toggle must not change
    // the answer. TrackResolver decides the same way via isEliteCymbalLane, and the
    // ghost preview has to agree with it or the preview lands somewhere else.
    SECTION("hi-hat, L-crash, ride and R-crash are cymbals regardless of the toggle")
    {
        for (uint lane : {2u, 3u, 7u, 8u})
        {
            REQUIRE(authorsCymbal(lane, Part::ELITE_DRUMS, false));
            REQUIRE(authorsCymbal(lane, Part::ELITE_DRUMS, true));
        }
    }

    SECTION("kick, snare and the toms are never cymbals regardless of the toggle")
    {
        for (uint lane : {0u, 1u, 4u, 5u, 6u, (uint)ELITE_KICK_2X_COLUMN})
        {
            REQUIRE(authorsCymbal(lane, Part::ELITE_DRUMS, false) == false);
            REQUIRE(authorsCymbal(lane, Part::ELITE_DRUMS, true) == false);
        }
    }

    SECTION("agrees with isEliteCymbalLane on every lane")
    {
        for (uint lane = 0; lane <= 8; ++lane)
            REQUIRE(authorsCymbal(lane, Part::ELITE_DRUMS, false) == isEliteCymbalLane(lane));
    }
}

// ============================================================================
// OptimisticPatchBuffer carries the gem

TEST_CASE("OptimisticPatchBuffer - adds carry their gem", "[patch_buffer]")
{
    // The optimistic add is what renders for the few frames between the click and the
    // reparse. Without the gem it defaulted to NOTE, so placing a cymbal drew at gemZ
    // and then jumped to cymZ when the real note arrived.
    OptimisticPatchBuffer buf;

    SECTION("the gem survives the round trip")
    {
        buf.addAdd(7, 4.0, Gem::CYM);
        REQUIRE(buf.getAdds().size() == 1);
        REQUIRE(buf.getAdds()[0].lane == 7);
        REQUIRE(buf.getAdds()[0].startQN == 4.0);
        REQUIRE(buf.getAdds()[0].gem == Gem::CYM);
    }

    SECTION("each add keeps its own gem")
    {
        buf.addAdd(1, 0.0, Gem::NOTE);
        buf.addAdd(2, 0.0, Gem::CYM_ACCENT);
        REQUIRE(buf.getAdds()[0].gem == Gem::NOTE);
        REQUIRE(buf.getAdds()[1].gem == Gem::CYM_ACCENT);
    }
}
