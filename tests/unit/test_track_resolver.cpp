#include "test_helpers.h"
#include "Midi/Processing/TrackResolver.h"

using EliteDrums = MidiPitchDefinitions::EliteDrums;

namespace
{
    // One note on/off pair on `pitch`, in the shape extract() walks.
    NoteStateMapArray oneNote(uint pitch, double onQN, double offQN, uint8_t velocity = 100)
    {
        NoteStateMapArray notes;
        notes[pitch][PPQ(onQN)]  = { velocity };
        notes[pitch][PPQ(offQN)] = { 0 };
        return notes;
    }

    SharedWindow extractElite(const NoteStateMapArray& notes)
    {
        return TrackResolver::extract(notes, PPQ(0.0), PPQ(16.0), PPQ(16.0), false, true);
    }
}

// ============================================================================
// Elite roll-lane pitches that collide with guitar/drums modifier pitches.
//
// extract() dispatches modifiers through one if/else chain that tests the guitar and
// 4-lane drum pitches BEFORE the elite roll-lane branch. Four elite roll pitches are
// numerically identical to an earlier test, so they were swallowed: the roll lane
// vanished AND a phantom modifier appeared in its place.

TEST_CASE("TrackResolver - elite roll lanes survive colliding modifier pitches",
          "[track_resolver][elite]")
{
    struct Case { uint pitch; const char* name; };
    const Case collisions[] = {
        { (uint)EliteDrums::ROLL_KICK,  "110, same as Drums::TOM_YELLOW" },
        { (uint)EliteDrums::ROLL_SNARE, "111, same as Drums::TOM_BLUE" },
        { (uint)EliteDrums::ROLL_HIHAT, "112, same as Drums::TOM_GREEN" },
        { (uint)EliteDrums::ROLL_TOM3,  "116, same as Guitar::SP and Drums::SP" },
    };

    for (const auto& c : collisions)
    {
        SECTION(c.name)
        {
            auto shared = extractElite(oneNote(c.pitch, 1.0, 2.0));

            REQUIRE(shared.lanes.size() == 1);
            REQUIRE(shared.lanes[0].laneType == (uint8_t)c.pitch);

            // and nothing leaked into the modifier it collides with
            REQUIRE(shared.modifiers.starPower.empty());
            REQUIRE(shared.modifiers.tomYellow.empty());
            REQUIRE(shared.modifiers.tomBlue.empty());
            REQUIRE(shared.modifiers.tomGreen.empty());
        }
    }
}

TEST_CASE("TrackResolver - elite roll lanes that never collided still work",
          "[track_resolver][elite]")
{
    // Control: these four have no counterpart in the earlier branches, so they were
    // always fine. If a fix breaks them it broke the roll branch itself.
    for (uint pitch : { (uint)EliteDrums::ROLL_LCRASH, (uint)EliteDrums::ROLL_TOM1,
                        (uint)EliteDrums::ROLL_TOM2,   (uint)EliteDrums::ROLL_RCRASH })
    {
        auto shared = extractElite(oneNote(pitch, 1.0, 2.0));
        REQUIRE(shared.lanes.size() == 1);
        REQUIRE(shared.lanes[0].laneType == (uint8_t)pitch);
    }
}

TEST_CASE("TrackResolver - elite star power still parses", "[track_resolver][elite]")
{
    // EliteDrums::SP is 104, which is Guitar::TAP. The SP branch tests its elite clause
    // first, so this one was already correct. Pin it so a reorder can't break it.
    auto shared = extractElite(oneNote((uint)EliteDrums::SP, 1.0, 4.0));
    REQUIRE(shared.modifiers.starPower.size() == 1);
    REQUIRE(shared.modifiers.tap.empty());
    REQUIRE(shared.lanes.empty());
}

TEST_CASE("TrackResolver - 4-lane tom markers are untouched", "[track_resolver][drums]")
{
    // The same pitches on a NON-elite track must still be tom markers, not lanes.
    auto shared = TrackResolver::extract(oneNote(110, 1.0, 2.0), PPQ(0.0), PPQ(16.0),
                                         PPQ(16.0), false, /*isElite=*/false);
    REQUIRE(shared.modifiers.tomYellow.size() == 1);
    REQUIRE(shared.lanes.empty());
}
