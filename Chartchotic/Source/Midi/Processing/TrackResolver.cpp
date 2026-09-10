/*
  ==============================================================================

    TrackResolver.cpp
    Pure-data resolve pipeline: SharedWindow + config → PartWindow.

  ==============================================================================
*/

#include "TrackResolver.h"
#include "../Utils/MidiConstants.h"

//==============================================================================
// Extract
//==============================================================================

SharedWindow TrackResolver::extract(const NoteStateMapArray& notes,
                                    PPQ windowStart, PPQ windowEnd, PPQ latencyEnd,
                                    bool bemaniMode, bool isElite)
{
    using Guitar = MidiPitchDefinitions::Guitar;
    using Drums = MidiPitchDefinitions::Drums;

    SharedWindow shared;

    for (uint pitch = MIDI_PITCH_MIN; pitch < MIDI_PITCH_COUNT; pitch++)
    {
        const NoteStateMap& nsm = notes[pitch];
        bool isMod = InstrumentMapper::isModifier(pitch, isElite);

        if (isMod)
        {
            PPQ onPPQ = PPQ(-1.0);
            uint8_t onVelocity = 0;
            for (auto it = nsm.begin(); it != nsm.end(); ++it)
            {
                if (it->second.velocity > 0)
                {
                    onPPQ = it->first;
                    onVelocity = it->second.velocity;
                }
                else if (onPPQ >= PPQ(0.0))
                {
                    ModifierRange range{onPPQ, it->first};

                    // Elite reuses pitches that mean something else on guitar / 4-lane
                    // drums: its roll lanes 110/111/112 are the tom markers and 116 is
                    // star power. On an elite track the elite meaning wins, so these are
                    // tested BEFORE the shared branches. Ordering them after silently ate
                    // the lane and invented a phantom tom marker or SP phrase.
                    const bool eliteRoll = isElite && InstrumentMapper::isEliteRollLane(pitch);
                    const int hatPedal   = isElite ? InstrumentMapper::eliteHiHatPedalSkillIndex(pitch) : -1;
                    const int hatIndiff  = isElite ? InstrumentMapper::eliteHiHatIndifferentSkillIndex(pitch) : -1;
                    const int flamSkill  = isElite ? InstrumentMapper::eliteFlamSkillIndex(pitch) : -1;
                    const bool laneMarker = eliteRoll
                        || pitch == (uint)Guitar::LANE_1 || pitch == (uint)Drums::LANE_1
                        || pitch == (uint)Guitar::LANE_2 || pitch == (uint)Drums::LANE_2;

                    if (laneMarker)
                    {
                        PPQ extStart = bemaniMode ? onPPQ : onPPQ - MIDI_LANE_EXTENSION_TIME;
                        auto onIt = nsm.find(onPPQ);
                        uint8_t laneVel = (onIt != nsm.end()) ? onIt->second.velocity : 100;
                        shared.lanes.push_back({extStart, it->first, (uint8_t)pitch, laneVel});
                    }
                    else if (hatPedal >= 0)
                    {
                        // The range drives closed-hat detection, the note keeps the velocity.
                        shared.modifiers.hihatPedal[hatPedal].push_back(range);
                        shared.hihatPedalNotes[hatPedal].push_back({onPPQ, it->first, onVelocity});
                    }
                    else if (hatIndiff >= 0) shared.modifiers.hihatIndifferent[hatIndiff].push_back(range);
                    else if (flamSkill >= 0) shared.modifiers.flam[flamSkill].push_back(range);
                    else if (pitch == (uint)Guitar::SP || pitch == (uint)Drums::SP
                        || (isElite && pitch == (uint)MidiPitchDefinitions::EliteDrums::SP))
                        shared.modifiers.starPower.push_back(range);
                    else if (pitch == (uint)Guitar::TAP)
                        shared.modifiers.tap.push_back(range);
                    else if (pitch == (uint)Drums::TOM_YELLOW)
                        shared.modifiers.tomYellow.push_back(range);
                    else if (pitch == (uint)Drums::TOM_BLUE)
                        shared.modifiers.tomBlue.push_back(range);
                    else if (pitch == (uint)Drums::TOM_GREEN)
                        shared.modifiers.tomGreen.push_back(range);
                    else if (pitch == (uint)Guitar::EASY_HOPO)     shared.modifiers.hopoForce[0].push_back(range);
                    else if (pitch == (uint)Guitar::MEDIUM_HOPO)   shared.modifiers.hopoForce[1].push_back(range);
                    else if (pitch == (uint)Guitar::HARD_HOPO)     shared.modifiers.hopoForce[2].push_back(range);
                    else if (pitch == (uint)Guitar::EXPERT_HOPO)   shared.modifiers.hopoForce[3].push_back(range);
                    else if (pitch == (uint)Guitar::EASY_STRUM)    shared.modifiers.strumForce[0].push_back(range);
                    else if (pitch == (uint)Guitar::MEDIUM_STRUM)  shared.modifiers.strumForce[1].push_back(range);
                    else if (pitch == (uint)Guitar::HARD_STRUM)    shared.modifiers.strumForce[2].push_back(range);
                    else if (pitch == (uint)Guitar::EXPERT_STRUM)  shared.modifiers.strumForce[3].push_back(range);

                    onPPQ = PPQ(-1.0);
                }
            }
        }
        else
        {
            for (auto it = nsm.begin(); it != nsm.end(); ++it)
            {
                if (it->second.velocity > 0)
                {
                    PPQ notePPQ = it->first;

                    if (notePPQ >= windowStart && notePPQ < windowEnd)
                        shared.positions[notePPQ].push_back({(uint8_t)pitch, it->second.velocity});

                    // Find note-off for sustain pairing
                    auto nextIt = std::next(it);
                    while (nextIt != nsm.end() && nextIt->second.velocity != 0)
                        ++nextIt;
                    PPQ offPPQ = (nextIt != nsm.end()) ? nextIt->first : latencyEnd;

                    if (notePPQ < windowEnd && offPPQ > windowStart)
                        shared.sustains.push_back({notePPQ, offPPQ, (uint8_t)pitch, it->second.velocity});
                }
            }
        }
    }

    return shared;
}

//==============================================================================
// Resolve (top-level)
//==============================================================================

PartWindow TrackResolver::resolve(const SharedWindow& shared, const Config& cfg)
{
    bool isGuitar = isGuitarLike(cfg.part);
    bool isDrums = isDrumLike(cfg.part);
    bool isElite = getRenderType(cfg.part) == RenderType::ELITE_DRUMS;

    std::array<DiffContext, 4> diffs;
    for (int i = 0; i < 4; i++)
    {
        diffs[i].skill = (SkillLevel)(i + 1);
        diffs[i].idx = i;
        if (isGuitar)
            diffs[i].playablePitches = InstrumentMapper::getGuitarPitchesForSkill(diffs[i].skill);
        else if (isElite)
            diffs[i].playablePitches = InstrumentMapper::getEliteDrumPitchesForSkill(diffs[i].skill);
        else if (isDrums)
            diffs[i].playablePitches = InstrumentMapper::getDrumPitchesForSkill(diffs[i].skill);
    }

    PartWindow result;
    resolveNotes(result, shared, cfg, diffs);
    resolveHiHatPedalGems(result, shared, cfg, diffs);   // after notes: reads the resolved hat state
    resolveSustains(result, shared, cfg, diffs);
    resolveLanes(result, shared, cfg, diffs);
    resolveHiHatSustains(result, shared, cfg, diffs);   // after pedal gems: reads Open-hat + Splash gems
    return result;
}

//==============================================================================
// Notes → TrackWindow
//==============================================================================

void TrackResolver::resolveNotes(PartWindow& result,
                                 const SharedWindow& shared,
                                 const Config& cfg,
                                 const std::array<DiffContext, 4>& diffs)
{
    using Drums = MidiPitchDefinitions::Drums;
    bool isGuitar = isGuitarLike(cfg.part);
    bool isDrums = isDrumLike(cfg.part);
    bool isElite = getRenderType(cfg.part) == RenderType::ELITE_DRUMS;

    std::array<PrevNote, 4> prevNotes;

    for (auto& [position, events] : shared.positions)
    {
        bool spActive = cfg.starPower && ModifierRanges::isActiveAt(shared.modifiers.starPower, position);

        for (auto& dc : diffs)
        {
            TrackFrame frame{};
            int noteCount = 0;
            uint singleColumn = LANE_COUNT;

            for (auto& evt : events)
            {
                bool playable = std::find(dc.playablePitches.begin(), dc.playablePitches.end(),
                                          (uint)evt.pitch) != dc.playablePitches.end();
                if (!playable) continue;

                uint gemColumn;
                if (isGuitar)
                    gemColumn = InstrumentMapper::getGuitarColumn(evt.pitch, dc.skill);
                else if (isElite)
                    gemColumn = InstrumentMapper::getEliteDrumColumn(evt.pitch, dc.skill, cfg.kick2x);
                else
                    gemColumn = InstrumentMapper::getDrumColumn(evt.pitch, dc.skill, cfg.kick2x);

                if (gemColumn >= LANE_COUNT) continue;

                noteCount++;
                singleColumn = gemColumn;

                Gem gemType = Gem::NOTE;
                if (isDrums)
                {
                    Dynamic dynamic = (Dynamic)evt.velocity;
                    bool cymbal = false;
                    if (isElite)
                    {
                        // Elite lanes are drum XOR cymbal, fixed by lane (no tom modifiers).
                        cymbal = isEliteCymbalLane(gemColumn);
                    }
                    else if (cfg.proDrums)
                    {
                        Drums note = (Drums)evt.pitch;
                        if (note == Drums::EASY_YELLOW || note == Drums::MEDIUM_YELLOW ||
                            note == Drums::HARD_YELLOW || note == Drums::EXPERT_YELLOW)
                            cymbal = !ModifierRanges::isActiveAt(shared.modifiers.tomYellow, position);
                        else if (note == Drums::EASY_BLUE || note == Drums::MEDIUM_BLUE ||
                                 note == Drums::HARD_BLUE || note == Drums::EXPERT_BLUE)
                            cymbal = !ModifierRanges::isActiveAt(shared.modifiers.tomBlue, position);
                        else if (note == Drums::EASY_GREEN || note == Drums::MEDIUM_GREEN ||
                                 note == Drums::HARD_GREEN || note == Drums::EXPERT_GREEN)
                            cymbal = !ModifierRanges::isActiveAt(shared.modifiers.tomGreen, position);
                    }

                    bool isKick = isElite ? isDrumKick(gemColumn, cfg.part)
                                          : InstrumentMapper::isDrumKick(evt.pitch);
                    // Elite kicks carry dynamics, 4-lane kicks don't: stock 4L kits send no
                    // kick velocity, which is also why the spec says kick dynamics are not
                    // carried down to 4L. Stomps and Splashes never have them either.
                    bool canHaveDynamics = cfg.dynamics && (isElite || !isKick);
                    gemType = GemCalculator::resolveDrumGem(cymbal, canHaveDynamics, dynamic);

                    // Disco flip (4-lane only; elite has its own MIDI disco marker, deferred)
                    if (!isElite && cfg.discoFlipState && cfg.proDrums && cfg.discoFlip &&
                        cfg.discoFlipState->isFlipped(position, dc.idx))
                    {
                        if (gemColumn == 1)
                        {
                            bool yellowTomActive = ModifierRanges::isActiveAt(shared.modifiers.tomYellow, position);
                            gemType = swapCymbalFlag(gemType, !yellowTomActive);
                            gemColumn = 2;
                        }
                        else if (gemColumn == 2)
                        {
                            gemType = swapCymbalFlag(gemType, false);
                            gemColumn = 1;
                        }
                    }
                }

                // Elite Hi-Hat (yellow cymbal) pedal state: default Open, coincident Pedal Down
                // makes it Closed, a coincident Indifferent marker makes it Indifferent (which
                // wins over Closed, per the ED spec's Appendix B interaction table).
                HiHatState hihat = HiHatState::None;
                if (isElite && isEliteHiHatLane(gemColumn))
                {
                    if (ModifierRanges::isActiveAt(shared.modifiers.hihatIndifferent[dc.idx], position))
                        hihat = HiHatState::Indifferent;
                    else if (ModifierRanges::isActiveAt(shared.modifiers.hihatPedal[dc.idx], position))
                        hihat = HiHatState::Closed;
                    else
                        hihat = HiHatState::Open;
                }

                frame[gemColumn] = GemWrapper(gemType, spActive, hihat);
            }

            if (noteCount == 0) continue;

            // Elite flam marker. The spec leaves multiple simultaneous hand gems undefined;
            // we take the leftmost hand gem (kicks are excluded — they flam as stacked
            // 1x + 2x) and drop the flam entirely if that gem sits inside a roll lane.
            if (isElite && ModifierRanges::isActiveAt(shared.modifiers.flam[dc.idx], position))
            {
                for (uint c = 0; c < LANE_COUNT; c++)
                {
                    if (frame[c].gem == Gem::NONE || isDrumKick(c, cfg.part)) continue;
                    if (!eliteRollLaneCovers(shared, c, position, dc.skill))
                        frame[c].flam = true;
                    break;
                }
            }

            bool isChord = (noteCount >= 2);

            // Guitar: resolve with chord/auto-HOPO context
            if (isGuitar)
            {
                bool hopoForced = ModifierRanges::isActiveAt(shared.modifiers.hopoForce[dc.idx], position);
                bool strumForced = ModifierRanges::isActiveAt(shared.modifiers.strumForce[dc.idx], position);
                bool tapForced = ModifierRanges::isActiveAt(shared.modifiers.tap, position);

                bool autoHOPO = false;
                if (cfg.autoHopo && !isChord && !prevNotes[dc.idx].wasChord)
                {
                    PPQ dist = position - prevNotes[dc.idx].position;
                    if (dist > PPQ(0.0) && dist <= cfg.hopoThreshold &&
                        singleColumn != prevNotes[dc.idx].column)
                    {
                        autoHOPO = true;
                    }
                }

                Gem resolvedGem = GemCalculator::resolveGuitarGem(isChord, autoHOPO, hopoForced, strumForced, tapForced);
                for (uint c = 0; c < LANE_COUNT; c++)
                {
                    if (frame[c].gem != Gem::NONE)
                        frame[c] = GemWrapper(resolvedGem, spActive);
                }
            }

            result.forSkill(dc.skill).trackWindow[position] = frame;

            prevNotes[dc.idx].position = position;
            prevNotes[dc.idx].column = isChord ? LANE_COUNT : singleColumn;
            prevNotes[dc.idx].wasChord = isChord;
        }
    }
}

//==============================================================================
// Sustains → SustainWindow
//==============================================================================

void TrackResolver::resolveSustains(PartWindow& result,
                                    const SharedWindow& shared,
                                    const Config& cfg,
                                    const std::array<DiffContext, 4>& diffs)
{
    if (!isGuitarLike(cfg.part)) return;

    for (auto& dc : diffs)
    {
        auto& sw = result.forSkill(dc.skill).sustainWindow;

        for (auto& sus : shared.sustains)
        {
            bool playable = std::find(dc.playablePitches.begin(), dc.playablePitches.end(),
                                      (uint)sus.pitch) != dc.playablePitches.end();
            if (!playable) continue;

            PPQ duration = sus.endPPQ - sus.startPPQ;
            if (duration < MIDI_MIN_SUSTAIN_LENGTH) continue;

            uint gemColumn = InstrumentMapper::getGuitarColumn(sus.pitch, dc.skill);
            if (gemColumn >= LANE_COUNT) continue;

            bool spActive = cfg.starPower && ModifierRanges::isActiveAt(shared.modifiers.starPower, sus.startPPQ);

            bool hopoForced = ModifierRanges::isActiveAt(shared.modifiers.hopoForce[dc.idx], sus.startPPQ);
            bool strumForced = ModifierRanges::isActiveAt(shared.modifiers.strumForce[dc.idx], sus.startPPQ);
            bool tapForced = ModifierRanges::isActiveAt(shared.modifiers.tap, sus.startPPQ);
            Gem gemType = GemCalculator::resolveGuitarGem(false, false, hopoForced, strumForced, tapForced);

            SustainEvent sustain;
            sustain.startPPQ = sus.startPPQ;
            sustain.endPPQ = sus.endPPQ;
            sustain.gemColumn = gemColumn;
            sustain.sustainType = SustainType::SUSTAIN;
            sustain.gemType = GemWrapper(gemType, spActive);
            sw.push_back(sustain);
        }
    }
}

//==============================================================================
// Lanes → SustainWindow
//==============================================================================

void TrackResolver::resolveLanes(PartWindow& result,
                                 const SharedWindow& shared,
                                 const Config& cfg,
                                 const std::array<DiffContext, 4>& diffs)
{
    using Guitar = MidiPitchDefinitions::Guitar;
    using Drums = MidiPitchDefinitions::Drums;
    bool isGuitar = isGuitarLike(cfg.part);
    bool isElite = getRenderType(cfg.part) == RenderType::ELITE_DRUMS;

    for (auto& dc : diffs)
    {
        auto& sw = result.forSkill(dc.skill).sustainWindow;

        for (auto& lane : shared.lanes)
        {
            if (!laneAppliesToSkill(lane, dc.skill)) continue;

            // Elite roll lanes carry their column in the pitch itself (110..118 -> 0..8),
            // one lane per pitch, so map directly instead of inferring from underlying notes.
            if (isElite)
            {
                uint col = InstrumentMapper::getEliteRollLaneColumn(lane.laneType);
                if (col >= LANE_COUNT) continue;
                SustainEvent laneEvent;
                laneEvent.startPPQ = lane.startPPQ;
                laneEvent.endPPQ = lane.endPPQ;
                laneEvent.gemColumn = col;
                laneEvent.sustainType = SustainType::LANE;
                laneEvent.gemType = GemWrapper(Gem::NOTE, false);
                sw.push_back(laneEvent);
                continue;
            }

            uint maxNotes = (lane.laneType == (uint8_t)Drums::LANE_2 ||
                             lane.laneType == (uint8_t)Guitar::LANE_2) ? 2u : 1u;

            // Find first notes in lane range to determine columns
            std::vector<std::pair<PPQ, uint>> noteEvents;
            for (auto& sus : shared.sustains)
            {
                bool playable = std::find(dc.playablePitches.begin(), dc.playablePitches.end(),
                                          (uint)sus.pitch) != dc.playablePitches.end();
                if (!playable) continue;
                if (sus.startPPQ >= lane.startPPQ && sus.startPPQ <= lane.endPPQ)
                {
                    uint col = isGuitar ? InstrumentMapper::getGuitarColumn(sus.pitch, dc.skill)
                                        : InstrumentMapper::getDrumColumn(sus.pitch, dc.skill, cfg.kick2x);
                    if (col < LANE_COUNT)
                        noteEvents.push_back({sus.startPPQ, col});
                }
            }
            std::sort(noteEvents.begin(), noteEvents.end());

            std::vector<uint> laneColumns;
            for (size_t i = 0; i < noteEvents.size() && laneColumns.size() < maxNotes; ++i)
                laneColumns.push_back(noteEvents[i].second);

            for (uint col : laneColumns)
            {
                SustainEvent laneEvent;
                laneEvent.startPPQ = lane.startPPQ;
                laneEvent.endPPQ = lane.endPPQ;
                laneEvent.gemColumn = col;
                laneEvent.sustainType = SustainType::LANE;
                laneEvent.gemType = GemWrapper(Gem::NOTE, false);
                sw.push_back(laneEvent);
            }
        }
    }
}

//==============================================================================
// Hi-hat pedal gems (Stomp / Splash)
//==============================================================================

void TrackResolver::resolveHiHatPedalGems(PartWindow& result,
                                          const SharedWindow& shared,
                                          const Config& cfg,
                                          const std::array<DiffContext, 4>& diffs)
{
    if (getRenderType(cfg.part) != RenderType::ELITE_DRUMS) return;

    for (auto& dc : diffs)
    {
        auto& tw = result.forSkill(dc.skill).trackWindow;

        for (const auto& pedal : shared.hihatPedalNotes[dc.idx])
        {
            // Velocity 1 draws nothing but still terminates a hi-hat sustain, so it is kept.
            if (pedal.velocity <= 1) continue;
            const bool splash = (pedal.velocity == 127);

            const bool strict = cfg.strictHatPedalState
                             && cfg.strictHatPedalState->isStrictAt(pedal.startPPQ);

            auto fit = tw.find(pedal.startPPQ);
            GemWrapper* hat = (fit != tw.end()) ? &fit->second[ELITE_HIHAT_COLUMN] : nullptr;
            const bool nonIndifferentYellow = hat && hat->gem != Gem::NONE
                                           && hat->hihat != HiHatState::Indifferent;

            if (!strict && nonIndifferentYellow) continue;

            // Strict pairs Splash with an Open hat, but resolveNotes closed it off this pedal.
            if (strict && splash && hat && hat->hihat == HiHatState::Closed)
                hat->hihat = HiHatState::Open;

            const uint col = splash ? (uint)ELITE_SPLASH_COLUMN : (uint)ELITE_STOMP_COLUMN;
            tw[pedal.startPPQ][col] = GemWrapper(splash ? Gem::SPLASH : Gem::STOMP);
        }
    }
}

//==============================================================================
// Hi-hat sustains (ringing zones from Open Hi-Hat / Splash gems)
//==============================================================================

void TrackResolver::resolveHiHatSustains(PartWindow& result,
                                         const SharedWindow& shared,
                                         const Config& cfg,
                                         const std::array<DiffContext, 4>& diffs)
{
    if (getRenderType(cfg.part) != RenderType::ELITE_DRUMS) return;

    const PPQ SIXTEENTH(0.25), EIGHTH(0.5), QUARTER(1.0), DOTTED_QUARTER(1.5);
    const PPQ NEVER(1e12);

    for (auto& dc : diffs)
    {
        auto& diffWindow = result.forSkill(dc.skill);
        auto& tw = diffWindow.trackWindow;

        // Note-offs sit one tick short (NoteProcessor::addNoteToMap). The 1/8 rule boundary is an
        // equality test, so the length has to come back exact.
        const PPQ oneTick(1);
        auto noteLenAtColumn = [&](PPQ pos, uint wantCol) -> PPQ {
            for (const auto& sus : shared.sustains)
            {
                if (sus.startPPQ != pos) continue;
                bool playable = std::find(dc.playablePitches.begin(), dc.playablePitches.end(),
                                          (uint)sus.pitch) != dc.playablePitches.end();
                if (!playable) continue;
                if (InstrumentMapper::getEliteDrumColumn(sus.pitch, dc.skill, cfg.kick2x) == wantCol)
                    return sus.endPPQ - sus.startPPQ + oneTick;
            }
            return PPQ(0.0);
        };
        auto pedalLenAt = [&](PPQ pos) -> PPQ {
            for (const auto& p : shared.hihatPedalNotes[dc.idx])
                if (p.startPPQ == pos) return p.endPPQ - p.startPPQ + oneTick;
            return PPQ(0.0);
        };

        struct Gen { PPQ start; PPQ len; bool splash; };
        std::vector<Gen> gens;
        std::vector<PPQ> terminators;

        for (const auto& [pos, frame] : tw)
        {
            const GemWrapper& hat = frame[ELITE_HIHAT_COLUMN];
            bool openHat   = (hat.gem != Gem::NONE && hat.hihat == HiHatState::Open);
            bool closedHat = (hat.gem != Gem::NONE && hat.hihat == HiHatState::Closed);

            if (openHat)
                gens.push_back({pos, noteLenAtColumn(pos, (uint)ELITE_HIHAT_COLUMN), false});
            if (frame[ELITE_SPLASH_COLUMN].gem == Gem::SPLASH)
                gens.push_back({pos, pedalLenAt(pos), true});
            if (openHat || closedHat)
                terminators.push_back(pos);
        }
        // Velocity 1 draws no gem but still terminates.
        for (const auto& p : shared.hihatPedalNotes[dc.idx])
            terminators.push_back(p.startPPQ);

        if (gens.empty()) continue;
        std::sort(gens.begin(), gens.end(), [](const Gen& a, const Gen& b) { return a.start < b.start; });
        std::sort(terminators.begin(), terminators.end());

        auto firstTerminatorAfter = [&](PPQ s) -> PPQ {
            auto it = std::upper_bound(terminators.begin(), terminators.end(), s);
            return (it != terminators.end()) ? *it : NEVER;
        };

        struct Seg { PPQ start; PPQ end; PPQ fadeStart; bool splash; };
        std::vector<Seg> segs;
        for (const auto& g : gens)
        {
            PPQ term = firstTerminatorAfter(g.start);
            Seg seg;
            seg.start = g.start;
            seg.splash = g.splash;
            if (g.len >= EIGHTH)
            {
                // Manual: match the notated length, last 1/8 fades. An interior terminator cuts clean.
                PPQ notated = g.start + g.len;
                if (term < notated) { seg.end = term;    seg.fadeStart = term; }
                else                { seg.end = notated; seg.fadeStart = notated - EIGHTH; }
            }
            else
            {
                // Default: look a dotted 1/4 ahead for a terminator, else 1/4 solid + 1/8 fade.
                if (term <= g.start + DOTTED_QUARTER) { seg.end = term;                     seg.fadeStart = term; }
                else                                  { seg.end = g.start + DOTTED_QUARTER; seg.fadeStart = g.start + QUARTER; }
            }
            segs.push_back(seg);
        }

        // Renewed segments are one ring, so the 1/16 floor applies to the whole run.
        auto& sw = diffWindow.sustainWindow;
        size_t i = 0;
        while (i < segs.size())
        {
            size_t j = i;
            while (j + 1 < segs.size()
                   && segs[j].fadeStart == segs[j].end
                   && segs[j].end == segs[j + 1].start)
                ++j;

            if (segs[j].end - segs[i].start > SIXTEENTH)
            {
                for (size_t k = i; k <= j; ++k)
                {
                    SustainEvent e;
                    e.startPPQ = segs[k].start;
                    e.endPPQ = segs[k].end;
                    e.gemColumn = (uint)ELITE_HIHAT_COLUMN;
                    e.sustainType = SustainType::HIHAT;
                    e.gemType = GemWrapper(segs[k].splash ? Gem::SPLASH : Gem::NOTE);
                    e.fadeStartPPQ = segs[k].fadeStart;
                    sw.push_back(e);
                }
            }
            i = j + 1;
        }
    }
}

//==============================================================================
// Helpers
//==============================================================================

bool TrackResolver::laneAppliesToSkill(const RawLaneMarker& lane, SkillLevel skill)
{
    if (skill == SkillLevel::EXPERT) return true;
    return skill == SkillLevel::HARD
        && lane.laneVelocity >= MIDI_LANE_HARD_VELOCITY_MIN
        && lane.laneVelocity <= MIDI_LANE_HARD_VELOCITY_MAX;
}

bool TrackResolver::eliteRollLaneCovers(const SharedWindow& shared, uint gemColumn,
                                        PPQ position, SkillLevel skill)
{
    for (const auto& lane : shared.lanes)
    {
        if (!laneAppliesToSkill(lane, skill)) continue;
        if (InstrumentMapper::getEliteRollLaneColumn(lane.laneType) != gemColumn) continue;
        if (position >= lane.startPPQ && position <= lane.endPPQ) return true;
    }
    return false;
}

Gem TrackResolver::swapCymbalFlag(Gem gem, bool cymbal)
{
    switch (gem)
    {
        case Gem::CYM_GHOST:
        case Gem::HOPO_GHOST:  return cymbal ? Gem::CYM_GHOST  : Gem::HOPO_GHOST;
        case Gem::CYM:
        case Gem::NOTE:        return cymbal ? Gem::CYM        : Gem::NOTE;
        case Gem::CYM_ACCENT:
        case Gem::TAP_ACCENT:  return cymbal ? Gem::CYM_ACCENT : Gem::TAP_ACCENT;
        default:               return gem;
    }
}
