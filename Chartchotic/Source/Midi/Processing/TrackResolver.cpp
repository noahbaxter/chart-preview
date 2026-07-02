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
            for (auto it = nsm.begin(); it != nsm.end(); ++it)
            {
                if (it->second.velocity > 0)
                {
                    onPPQ = it->first;
                }
                else if (onPPQ >= PPQ(0.0))
                {
                    ModifierRange range{onPPQ, it->first};

                    if (pitch == (uint)Guitar::SP || pitch == (uint)Drums::SP
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
                    else if (pitch == (uint)Guitar::LANE_1 || pitch == (uint)Drums::LANE_1 ||
                             pitch == (uint)Guitar::LANE_2 || pitch == (uint)Drums::LANE_2)
                    {
                        PPQ extStart = bemaniMode ? onPPQ : onPPQ - MIDI_LANE_EXTENSION_TIME;
                        auto onIt = nsm.find(onPPQ);
                        uint8_t laneVel = (onIt != nsm.end()) ? onIt->second.velocity : 100;
                        shared.lanes.push_back({extStart, it->first, (uint8_t)pitch, laneVel});
                    }

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
    resolveSustains(result, shared, cfg, diffs);
    resolveLanes(result, shared, cfg, diffs);
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
                    bool canHaveDynamics = cfg.dynamics && !isKick;
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

                frame[gemColumn] = GemWrapper(gemType, spActive);
            }

            if (noteCount == 0) continue;

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

    for (auto& dc : diffs)
    {
        auto& sw = result.forSkill(dc.skill).sustainWindow;

        for (auto& lane : shared.lanes)
        {
            bool appliesToSkill = (dc.skill == SkillLevel::EXPERT) ||
                                  (dc.skill == SkillLevel::HARD && lane.laneVelocity >= 41 && lane.laneVelocity <= 50);
            if (!appliesToSkill) continue;

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
// Helpers
//==============================================================================

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
