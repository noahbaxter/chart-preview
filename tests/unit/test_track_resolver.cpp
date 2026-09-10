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

    void addNote(NoteStateMapArray& notes, uint pitch, double onQN, double offQN,
                 uint8_t velocity = 100)
    {
        notes[pitch][PPQ(onQN)]  = { velocity };
        notes[pitch][PPQ(offQN)] = { 0 };
    }

    TrackResolver::Config eliteConfig()
    {
        TrackResolver::Config cfg;
        cfg.part = Part::ELITE_DRUMS;
        cfg.dynamics = true;
        cfg.kick2x = true;
        return cfg;
    }

    // The Expert frame at `onQN`, or nullptr if nothing resolved there.
    const TrackFrame* expertFrameAt(const PartWindow& pw, double onQN)
    {
        const auto& tw = pw.forSkill(SkillLevel::EXPERT).trackWindow;
        auto it = tw.find(PPQ(onQN));
        return (it == tw.end()) ? nullptr : &it->second;
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

// ============================================================================
// Elite flam marker (upper-octave Eb, per difficulty).

TEST_CASE("TrackResolver - elite flam markers parse per difficulty", "[track_resolver][elite]")
{
    struct Case { uint pitch; int skillIdx; };
    const Case cases[] = {
        { (uint)EliteDrums::EASY_FLAM,   0 },
        { (uint)EliteDrums::MEDIUM_FLAM, 1 },
        { (uint)EliteDrums::HARD_FLAM,   2 },
        { (uint)EliteDrums::EXPERT_FLAM, 3 },
    };

    for (const auto& c : cases)
    {
        auto shared = extractElite(oneNote(c.pitch, 1.0, 2.0));
        for (int i = 0; i < 4; i++)
            REQUIRE(shared.modifiers.flam[i].size() == (i == c.skillIdx ? 1u : 0u));
        REQUIRE(shared.positions.empty());   // a marker, not a gem
    }
}

TEST_CASE("TrackResolver - flam pitches stay guitar notes off an elite track",
          "[track_resolver][guitar]")
{
    // EXPERT_FLAM is 87, which is Guitar::HARD_BLUE. Off elite it must still be a note.
    auto shared = TrackResolver::extract(oneNote(87, 1.0, 2.0), PPQ(0.0), PPQ(16.0),
                                         PPQ(16.0), false, /*isElite=*/false);
    REQUIRE(shared.positions.size() == 1);
    for (int i = 0; i < 4; i++)
        REQUIRE(shared.modifiers.flam[i].empty());
}

TEST_CASE("TrackResolver - flam marker flams the hand gem under it", "[track_resolver][elite]")
{
    NoteStateMapArray notes;
    addNote(notes, (uint)EliteDrums::EXPERT_SNARE, 1.0, 1.1);
    addNote(notes, (uint)EliteDrums::EXPERT_FLAM,  1.0, 1.1);

    auto pw = TrackResolver::resolve(extractElite(notes), eliteConfig());
    const auto* frame = expertFrameAt(pw, 1.0);
    REQUIRE(frame != nullptr);
    REQUIRE((*frame)[1].gem != Gem::NONE);
    REQUIRE((*frame)[1].flam);
}

TEST_CASE("TrackResolver - flam keeps dynamics and hat state", "[track_resolver][elite]")
{
    SECTION("ghost snare stays a ghost")
    {
        NoteStateMapArray notes;
        addNote(notes, (uint)EliteDrums::EXPERT_SNARE, 1.0, 1.1, /*ghost velocity=*/1);
        addNote(notes, (uint)EliteDrums::EXPERT_FLAM,  1.0, 1.1);

        auto pw = TrackResolver::resolve(extractElite(notes), eliteConfig());
        const auto* frame = expertFrameAt(pw, 1.0);
        REQUIRE(frame != nullptr);
        REQUIRE((*frame)[1].gem == Gem::HOPO_GHOST);
        REQUIRE((*frame)[1].flam);
    }

    SECTION("open hi-hat stays open")
    {
        NoteStateMapArray notes;
        addNote(notes, (uint)EliteDrums::EXPERT_HIHAT, 1.0, 1.1);
        addNote(notes, (uint)EliteDrums::EXPERT_FLAM,  1.0, 1.1);

        auto pw = TrackResolver::resolve(extractElite(notes), eliteConfig());
        const auto* frame = expertFrameAt(pw, 1.0);
        REQUIRE(frame != nullptr);
        REQUIRE((*frame)[2].hihat == HiHatState::Open);
        REQUIRE((*frame)[2].flam);
    }
}

TEST_CASE("TrackResolver - flam never touches kicks", "[track_resolver][elite]")
{
    // Kicks flam as stacked 1x + 2x, so the marker must skip them. Here it should land on
    // the snare, not on either kick.
    NoteStateMapArray notes;
    addNote(notes, (uint)EliteDrums::EXPERT_KICK,    1.0, 1.1);
    addNote(notes, (uint)EliteDrums::EXPERT_KICK_2X, 1.0, 1.1);
    addNote(notes, (uint)EliteDrums::EXPERT_SNARE,   1.0, 1.1);
    addNote(notes, (uint)EliteDrums::EXPERT_FLAM,    1.0, 1.1);

    auto pw = TrackResolver::resolve(extractElite(notes), eliteConfig());
    const auto* frame = expertFrameAt(pw, 1.0);
    REQUIRE(frame != nullptr);
    REQUIRE_FALSE((*frame)[0].flam);
    REQUIRE_FALSE((*frame)[ELITE_KICK_2X_COLUMN].flam);
    REQUIRE((*frame)[1].flam);
}

TEST_CASE("TrackResolver - flam on simultaneous hand gems takes the leftmost",
          "[track_resolver][elite]")
{
    // The spec leaves this undefined; we pick the leftmost hand gem and pin it here.
    NoteStateMapArray notes;
    addNote(notes, (uint)EliteDrums::EXPERT_RIDE,  1.0, 1.1);
    addNote(notes, (uint)EliteDrums::EXPERT_HIHAT, 1.0, 1.1);
    addNote(notes, (uint)EliteDrums::EXPERT_FLAM,  1.0, 1.1);

    auto pw = TrackResolver::resolve(extractElite(notes), eliteConfig());
    const auto* frame = expertFrameAt(pw, 1.0);
    REQUIRE(frame != nullptr);
    REQUIRE((*frame)[2].flam);          // hi-hat, the leftmost
    REQUIRE_FALSE((*frame)[7].flam);    // ride
}

TEST_CASE("TrackResolver - flam inside a roll lane is ignored", "[track_resolver][elite]")
{
    NoteStateMapArray notes;
    addNote(notes, (uint)EliteDrums::EXPERT_SNARE, 1.0, 1.1);
    addNote(notes, (uint)EliteDrums::EXPERT_FLAM,  1.0, 1.1);
    addNote(notes, (uint)EliteDrums::ROLL_SNARE,   0.5, 2.0);   // covers the snare lane

    auto pw = TrackResolver::resolve(extractElite(notes), eliteConfig());
    const auto* frame = expertFrameAt(pw, 1.0);
    REQUIRE(frame != nullptr);
    REQUIRE((*frame)[1].gem != Gem::NONE);
    REQUIRE_FALSE((*frame)[1].flam);
}

TEST_CASE("TrackResolver - a roll lane on another column doesn't suppress the flam",
          "[track_resolver][elite]")
{
    NoteStateMapArray notes;
    addNote(notes, (uint)EliteDrums::EXPERT_SNARE, 1.0, 1.1);
    addNote(notes, (uint)EliteDrums::EXPERT_FLAM,  1.0, 1.1);
    addNote(notes, (uint)EliteDrums::ROLL_RIDE,    0.5, 2.0);

    auto pw = TrackResolver::resolve(extractElite(notes), eliteConfig());
    const auto* frame = expertFrameAt(pw, 1.0);
    REQUIRE(frame != nullptr);
    REQUIRE((*frame)[1].flam);
}
