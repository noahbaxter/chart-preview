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

    // A config whose [STRICT_HAT_PEDAL_STATE] latches from the very start of the chart.
    StrictHatPedalState strictFromZero()
    {
        TrackTextEvents events;
        events.push_back({PPQ(0.0), "[STRICT_HAT_PEDAL_STATE]"});
        StrictHatPedalState s;
        s.buildFromTextEvents(events);
        return s;
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

// ============================================================================
// Elite hi-hat pedal gems (Stomp / Splash).
// ============================================================================

TEST_CASE("TrackResolver - pedal velocity picks the gem", "[track_resolver][elite]")
{
    struct Case { uint8_t velocity; Gem gem; int column; const char* what; };
    const Case cases[] = {
        { 2,   Gem::STOMP,  ELITE_STOMP_COLUMN,  "lowest stomp velocity" },
        { 100, Gem::STOMP,  ELITE_STOMP_COLUMN,  "ordinary stomp" },
        { 126, Gem::STOMP,  ELITE_STOMP_COLUMN,  "highest stomp velocity" },
        { 127, Gem::SPLASH, ELITE_SPLASH_COLUMN, "splash" },
    };

    for (const auto& c : cases)
    {
        INFO(c.what);
        auto pw = TrackResolver::resolve(
            extractElite(oneNote((uint)EliteDrums::EXPERT_PEDAL, 1.0, 1.1, c.velocity)),
            eliteConfig());

        const auto* frame = expertFrameAt(pw, 1.0);
        REQUIRE(frame != nullptr);
        REQUIRE((*frame)[(uint)c.column].gem == c.gem);
    }
}

TEST_CASE("TrackResolver - pedal velocity 1 draws no gem", "[track_resolver][elite]")
{
    // Velocity 1 is a hi-hat sustain terminator only, so nothing lands on either pedal lane.
    auto pw = TrackResolver::resolve(
        extractElite(oneNote((uint)EliteDrums::EXPERT_PEDAL, 1.0, 1.1, 1)), eliteConfig());

    const auto* frame = expertFrameAt(pw, 1.0);
    if (frame != nullptr)
    {
        REQUIRE((*frame)[(uint)ELITE_STOMP_COLUMN].gem == Gem::NONE);
        REQUIRE((*frame)[(uint)ELITE_SPLASH_COLUMN].gem == Gem::NONE);
    }
}

TEST_CASE("TrackResolver - pedal gems land on every difficulty's own pitch",
          "[track_resolver][elite]")
{
    struct Case { uint pitch; SkillLevel skill; };
    const Case cases[] = {
        { (uint)EliteDrums::EASY_PEDAL,   SkillLevel::EASY },
        { (uint)EliteDrums::MEDIUM_PEDAL, SkillLevel::MEDIUM },
        { (uint)EliteDrums::HARD_PEDAL,   SkillLevel::HARD },
        { (uint)EliteDrums::EXPERT_PEDAL, SkillLevel::EXPERT },
    };

    for (const auto& c : cases)
    {
        auto pw = TrackResolver::resolve(extractElite(oneNote(c.pitch, 1.0, 1.1)), eliteConfig());
        const auto& tw = pw.forSkill(c.skill).trackWindow;
        auto it = tw.find(PPQ(1.0));
        REQUIRE(it != tw.end());
        REQUIRE(it->second[(uint)ELITE_STOMP_COLUMN].gem == Gem::STOMP);
    }
}

TEST_CASE("TrackResolver - a coincident yellow suppresses the pedal gem",
          "[track_resolver][elite]")
{
    // The pedal under a Yellow closes the hat; the bar itself is suppressed.
    NoteStateMapArray notes;
    addNote(notes, (uint)EliteDrums::EXPERT_HIHAT, 1.0, 1.1);
    addNote(notes, (uint)EliteDrums::EXPERT_PEDAL, 1.0, 1.1);

    auto pw = TrackResolver::resolve(extractElite(notes), eliteConfig());
    const auto* frame = expertFrameAt(pw, 1.0);
    REQUIRE(frame != nullptr);
    REQUIRE((*frame)[(uint)ELITE_HIHAT_COLUMN].hihat == HiHatState::Closed);
    REQUIRE((*frame)[(uint)ELITE_STOMP_COLUMN].gem == Gem::NONE);
}

TEST_CASE("TrackResolver - a yellow on another tick doesn't suppress", "[track_resolver][elite]")
{
    NoteStateMapArray notes;
    addNote(notes, (uint)EliteDrums::EXPERT_HIHAT, 2.0, 2.1);
    addNote(notes, (uint)EliteDrums::EXPERT_PEDAL, 1.0, 1.1);

    auto pw = TrackResolver::resolve(extractElite(notes), eliteConfig());
    const auto* frame = expertFrameAt(pw, 1.0);
    REQUIRE(frame != nullptr);
    REQUIRE((*frame)[(uint)ELITE_STOMP_COLUMN].gem == Gem::STOMP);
}

TEST_CASE("TrackResolver - an indifferent yellow never suppresses", "[track_resolver][elite]")
{
    NoteStateMapArray notes;
    addNote(notes, (uint)EliteDrums::EXPERT_HIHAT,        1.0, 1.1);
    addNote(notes, (uint)EliteDrums::EXPERT_INDIFFERENT,  1.0, 1.1);
    addNote(notes, (uint)EliteDrums::EXPERT_PEDAL,        1.0, 1.1);

    auto pw = TrackResolver::resolve(extractElite(notes), eliteConfig());
    const auto* frame = expertFrameAt(pw, 1.0);
    REQUIRE(frame != nullptr);
    REQUIRE((*frame)[(uint)ELITE_HIHAT_COLUMN].hihat == HiHatState::Indifferent);
    REQUIRE((*frame)[(uint)ELITE_STOMP_COLUMN].gem == Gem::STOMP);
}

TEST_CASE("TrackResolver - strict hat pedal state waives suppression",
          "[track_resolver][elite]")
{
    auto strict = strictFromZero();
    auto cfg = eliteConfig();
    cfg.strictHatPedalState = &strict;

    SECTION("closed hat and stomp render together")
    {
        NoteStateMapArray notes;
        addNote(notes, (uint)EliteDrums::EXPERT_HIHAT, 1.0, 1.1);
        addNote(notes, (uint)EliteDrums::EXPERT_PEDAL, 1.0, 1.1);

        auto pw = TrackResolver::resolve(extractElite(notes), cfg);
        const auto* frame = expertFrameAt(pw, 1.0);
        REQUIRE(frame != nullptr);
        REQUIRE((*frame)[(uint)ELITE_HIHAT_COLUMN].hihat == HiHatState::Closed);
        REQUIRE((*frame)[(uint)ELITE_STOMP_COLUMN].gem == Gem::STOMP);
    }

    SECTION("open hat and splash render together")
    {
        NoteStateMapArray notes;
        addNote(notes, (uint)EliteDrums::EXPERT_HIHAT, 1.0, 1.1);
        addNote(notes, (uint)EliteDrums::EXPERT_PEDAL, 1.0, 1.1, 127);

        auto pw = TrackResolver::resolve(extractElite(notes), cfg);
        const auto* frame = expertFrameAt(pw, 1.0);
        REQUIRE(frame != nullptr);
        REQUIRE((*frame)[(uint)ELITE_HIHAT_COLUMN].hihat == HiHatState::Open);
        REQUIRE((*frame)[(uint)ELITE_SPLASH_COLUMN].gem == Gem::SPLASH);
    }
}

TEST_CASE("TrackResolver - strict only applies from its own position onward",
          "[track_resolver][elite]")
{
    TrackTextEvents events;
    events.push_back({PPQ(4.0), "[STRICT_HAT_PEDAL_STATE]"});
    StrictHatPedalState strict;
    strict.buildFromTextEvents(events);

    auto cfg = eliteConfig();
    cfg.strictHatPedalState = &strict;

    NoteStateMapArray notes;
    addNote(notes, (uint)EliteDrums::EXPERT_HIHAT, 1.0, 1.1);
    addNote(notes, (uint)EliteDrums::EXPERT_PEDAL, 1.0, 1.1);
    addNote(notes, (uint)EliteDrums::EXPERT_HIHAT, 5.0, 5.1);
    addNote(notes, (uint)EliteDrums::EXPERT_PEDAL, 5.0, 5.1);

    auto pw = TrackResolver::resolve(extractElite(notes), cfg);

    const auto* before = expertFrameAt(pw, 1.0);
    REQUIRE(before != nullptr);
    REQUIRE((*before)[(uint)ELITE_STOMP_COLUMN].gem == Gem::NONE);

    const auto* after = expertFrameAt(pw, 5.0);
    REQUIRE(after != nullptr);
    REQUIRE((*after)[(uint)ELITE_STOMP_COLUMN].gem == Gem::STOMP);
}

TEST_CASE("TrackResolver - pedal gems stay off non-elite drums", "[track_resolver][elite]")
{
    // EXPERT_PEDAL is 72, an ordinary 4-lane pitch. Off elite nothing may reach col 10/11.
    auto shared = TrackResolver::extract(oneNote((uint)EliteDrums::EXPERT_PEDAL, 1.0, 1.1),
                                         PPQ(0.0), PPQ(16.0), PPQ(16.0), false, /*isElite=*/false);
    TrackResolver::Config cfg;
    cfg.part = Part::DRUMS;

    auto pw = TrackResolver::resolve(shared, cfg);
    const auto* frame = expertFrameAt(pw, 1.0);
    if (frame != nullptr)
    {
        REQUIRE((*frame)[(uint)ELITE_STOMP_COLUMN].gem == Gem::NONE);
        REQUIRE((*frame)[(uint)ELITE_SPLASH_COLUMN].gem == Gem::NONE);
    }
}

// ============================================================================
// Elite hi-hat sustains (ringing zones from Open Hi-Hat / Splash gems).
// ============================================================================

namespace
{
    // Every HIHAT sustain on Expert, in start order.
    std::vector<SustainEvent> hihatSustains(const PartWindow& pw)
    {
        std::vector<SustainEvent> out;
        for (const auto& s : pw.forSkill(SkillLevel::EXPERT).sustainWindow)
            if (s.sustainType == SustainType::HIHAT)
                out.push_back(s);
        std::sort(out.begin(), out.end(),
                  [](const SustainEvent& a, const SustainEvent& b) { return a.startPPQ < b.startPPQ; });
        return out;
    }

    // A note of exactly `lenQN`, accounting for the one-tick-short note-off convention.
    void addNoteOfLength(NoteStateMapArray& notes, uint pitch, double onQN, double lenQN,
                         uint8_t velocity = 100)
    {
        notes[pitch][PPQ(onQN)] = { velocity };
        notes[pitch][PPQ(onQN + lenQN) - PPQ(1)] = { 0 };
    }
}

TEST_CASE("TrackResolver - short open hat rings a dotted quarter with a fadeout",
          "[track_resolver][elite]")
{
    // Default rule: notated < 1/8 and no terminator within a dotted 1/4 gives
    // 1/4 solid + 1/8 fadeout.
    NoteStateMapArray notes;
    addNoteOfLength(notes, (uint)EliteDrums::EXPERT_HIHAT, 1.0, 0.1);

    auto sus = hihatSustains(TrackResolver::resolve(extractElite(notes), eliteConfig()));
    REQUIRE(sus.size() == 1);
    REQUIRE(sus[0].startPPQ == PPQ(1.0));
    REQUIRE(sus[0].endPPQ == PPQ(2.5));
    REQUIRE(sus[0].fadeStartPPQ == PPQ(2.0));
}

TEST_CASE("TrackResolver - long open hat matches its note length", "[track_resolver][elite]")
{
    // Manual rule: notated >= 1/8 rings for the notated length, last 1/8 fading.
    NoteStateMapArray notes;
    addNoteOfLength(notes, (uint)EliteDrums::EXPERT_HIHAT, 1.0, 2.0);

    auto sus = hihatSustains(TrackResolver::resolve(extractElite(notes), eliteConfig()));
    REQUIRE(sus.size() == 1);
    REQUIRE(sus[0].endPPQ == PPQ(3.0));
    REQUIRE(sus[0].fadeStartPPQ == PPQ(2.5));
}

TEST_CASE("TrackResolver - a pedal terminates a ringing hat cleanly", "[track_resolver][elite]")
{
    // Any Pedal Down note-on cuts the ring, with no fadeout tail.
    NoteStateMapArray notes;
    addNoteOfLength(notes, (uint)EliteDrums::EXPERT_HIHAT, 1.0, 0.1);
    addNote(notes, (uint)EliteDrums::EXPERT_PEDAL, 1.75, 1.85);

    auto sus = hihatSustains(TrackResolver::resolve(extractElite(notes), eliteConfig()));
    REQUIRE(sus.size() == 1);
    REQUIRE(sus[0].endPPQ == PPQ(1.75));
    REQUIRE(sus[0].fadeStartPPQ == sus[0].endPPQ);   // clean, no tail
}

TEST_CASE("TrackResolver - a velocity 1 pedal still terminates", "[track_resolver][elite]")
{
    // Velocity 1 draws no gem but is explicitly still a terminator.
    NoteStateMapArray notes;
    addNoteOfLength(notes, (uint)EliteDrums::EXPERT_HIHAT, 1.0, 0.1);
    addNote(notes, (uint)EliteDrums::EXPERT_PEDAL, 1.5, 1.6, /*velocity=*/1);

    auto sus = hihatSustains(TrackResolver::resolve(extractElite(notes), eliteConfig()));
    REQUIRE(sus.size() == 1);
    REQUIRE(sus[0].endPPQ == PPQ(1.5));
    REQUIRE(sus[0].fadeStartPPQ == sus[0].endPPQ);
}

TEST_CASE("TrackResolver - an interior pedal cuts a long hat short", "[track_resolver][elite]")
{
    NoteStateMapArray notes;
    addNoteOfLength(notes, (uint)EliteDrums::EXPERT_HIHAT, 1.0, 2.0);
    addNote(notes, (uint)EliteDrums::EXPERT_PEDAL, 2.0, 2.1);

    auto sus = hihatSustains(TrackResolver::resolve(extractElite(notes), eliteConfig()));
    REQUIRE(sus.size() == 1);
    REQUIRE(sus[0].endPPQ == PPQ(2.0));
    REQUIRE(sus[0].fadeStartPPQ == sus[0].endPPQ);
}

TEST_CASE("TrackResolver - a ring shorter than a sixteenth is omitted", "[track_resolver][elite]")
{
    // Terminated almost immediately, so the whole run is under the 1/16 floor.
    NoteStateMapArray notes;
    addNoteOfLength(notes, (uint)EliteDrums::EXPERT_HIHAT, 1.0, 0.1);
    addNote(notes, (uint)EliteDrums::EXPERT_PEDAL, 1.125, 1.2);

    auto sus = hihatSustains(TrackResolver::resolve(extractElite(notes), eliteConfig()));
    REQUIRE(sus.empty());
}

TEST_CASE("TrackResolver - a splash generates its own ring", "[track_resolver][elite]")
{
    // A lone v127 pedal emits a Splash gem, which is itself a generator.
    NoteStateMapArray notes;
    addNoteOfLength(notes, (uint)EliteDrums::EXPERT_PEDAL, 1.0, 0.1, /*velocity=*/127);

    auto sus = hihatSustains(TrackResolver::resolve(extractElite(notes), eliteConfig()));
    REQUIRE(sus.size() == 1);
    REQUIRE(sus[0].gemType.gem == Gem::SPLASH);   // renderer colours splash rings differently
    REQUIRE(sus[0].endPPQ == PPQ(2.5));
}

TEST_CASE("TrackResolver - a suppressed splash rings nothing", "[track_resolver][elite]")
{
    // The generators are the RESOLVED gems, so Feature 2's suppression removes the ring too.
    // The coincident Yellow is Closed here, so it is not an Open generator either.
    NoteStateMapArray notes;
    addNoteOfLength(notes, (uint)EliteDrums::EXPERT_HIHAT, 1.0, 0.1);
    addNoteOfLength(notes, (uint)EliteDrums::EXPERT_PEDAL, 1.0, 0.1, /*velocity=*/127);

    auto pw = TrackResolver::resolve(extractElite(notes), eliteConfig());
    const auto* frame = expertFrameAt(pw, 1.0);
    REQUIRE(frame != nullptr);
    REQUIRE((*frame)[(uint)ELITE_SPLASH_COLUMN].gem == Gem::NONE);
    REQUIRE(hihatSustains(pw).empty());
}

TEST_CASE("TrackResolver - strict open plus splash rings from both", "[track_resolver][elite]")
{
    // Under strict the hat stays Open AND the Splash survives, so the spec's two generators
    // both land on the same tick.
    auto strict = strictFromZero();
    auto cfg = eliteConfig();
    cfg.strictHatPedalState = &strict;

    NoteStateMapArray notes;
    addNoteOfLength(notes, (uint)EliteDrums::EXPERT_HIHAT, 1.0, 0.1);
    addNoteOfLength(notes, (uint)EliteDrums::EXPERT_PEDAL, 1.0, 0.1, /*velocity=*/127);

    auto sus = hihatSustains(TrackResolver::resolve(extractElite(notes), cfg));
    REQUIRE(sus.size() == 2);
    bool sawOpen = false, sawSplash = false;
    for (const auto& s : sus)
    {
        if (s.gemType.gem == Gem::SPLASH) sawSplash = true;
        if (s.gemType.gem == Gem::NOTE)   sawOpen = true;
    }
    REQUIRE(sawOpen);
    REQUIRE(sawSplash);
}

TEST_CASE("TrackResolver - a closed hat terminates but never generates",
          "[track_resolver][elite]")
{
    // Closed is a terminator only. The second hat here is closed by its own pedal, so it cuts
    // the first ring and starts nothing.
    NoteStateMapArray notes;
    addNoteOfLength(notes, (uint)EliteDrums::EXPERT_HIHAT, 1.0, 0.1);
    addNoteOfLength(notes, (uint)EliteDrums::EXPERT_HIHAT, 2.0, 0.1);
    addNote(notes, (uint)EliteDrums::EXPERT_PEDAL, 2.0, 2.1);

    auto sus = hihatSustains(TrackResolver::resolve(extractElite(notes), eliteConfig()));
    REQUIRE(sus.size() == 1);
    REQUIRE(sus[0].startPPQ == PPQ(1.0));
    REQUIRE(sus[0].endPPQ == PPQ(2.0));
}

TEST_CASE("TrackResolver - hi-hat sustains stay off non-elite drums", "[track_resolver][drums]")
{
    auto shared = TrackResolver::extract(oneNote((uint)EliteDrums::EXPERT_HIHAT, 1.0, 1.1),
                                         PPQ(0.0), PPQ(16.0), PPQ(16.0), false, /*isElite=*/false);
    TrackResolver::Config cfg;
    cfg.part = Part::DRUMS;

    REQUIRE(hihatSustains(TrackResolver::resolve(shared, cfg)).empty());
}
