/*
  ==============================================================================

    TrackResolver.h
    Pure-data resolve pipeline: SharedWindow + config → PartWindow.
    No locks, no ValueTree, no noteStateMapArray access.

  ==============================================================================
*/

#pragma once

#include "SharedTrackData.h"
#include "PartWindow.h"
#include "../Utils/InstrumentMapper.h"
#include "../Utils/GemCalculator.h"
#include "../DiscoFlipState.h"
#include "../StrictHatPedalState.h"
#include "../../Utils/ChartTypes.h"

class TrackResolver
{
public:
    // All settings needed for resolution — extracted from ValueTree once
    struct Config {
        Part part = Part::GUITAR;
        bool autoHopo = false;
        PPQ hopoThreshold = PPQ(0.0);
        bool dynamics = false;
        bool proDrums = false;
        bool kick2x = false;
        bool discoFlip = false;
        bool starPower = false;
        bool bemaniMode = false;
        const DiscoFlipState* discoFlipState = nullptr;
        const StrictHatPedalState* strictHatPedalState = nullptr;   // elite [STRICT_HAT_PEDAL_STATE]
    };

    // Full resolve: SharedWindow + config → PartWindow (all 4 difficulties)
    static PartWindow resolve(const SharedWindow& shared, const Config& cfg);

    // Extract SharedWindow from NoteStateMapArray (caller holds lock)
    static SharedWindow extract(const NoteStateMapArray& notes,
                                PPQ windowStart, PPQ windowEnd, PPQ latencyEnd,
                                bool bemaniMode = false, bool isElite = false);

private:
    // Per-difficulty context used during resolution
    struct DiffContext {
        SkillLevel skill;
        int idx; // 0..3
        std::vector<uint> playablePitches;
    };

    // Previous note tracking for auto-HOPO
    struct PrevNote {
        PPQ position = PPQ(-1000.0);
        uint column = LANE_COUNT;
        bool wasChord = false;
    };

    static void resolveNotes(PartWindow& result,
                             const SharedWindow& shared,
                             const Config& cfg,
                             const std::array<DiffContext, 4>& diffs);

    static void resolveSustains(PartWindow& result,
                                const SharedWindow& shared,
                                const Config& cfg,
                                const std::array<DiffContext, 4>& diffs);

    static void resolveLanes(PartWindow& result,
                             const SharedWindow& shared,
                             const Config& cfg,
                             const std::array<DiffContext, 4>& diffs);

    // Elite Pedal Down notes → Stomp/Splash gems, suppressed by a coincident Yellow unless strict.
    static void resolveHiHatPedalGems(PartWindow& result,
                                      const SharedWindow& shared,
                                      const Config& cfg,
                                      const std::array<DiffContext, 4>& diffs);

    // Disco flip: replace cymbal/tom flag while preserving dynamic
    static Gem swapCymbalFlag(Gem gem, bool cymbal);

    // Whether a lane marker is visible on this difficulty: Expert always, Hard only for the
    // velocity 41-50 "also on Hard" encoding.
    static bool laneAppliesToSkill(const RawLaneMarker& lane, SkillLevel skill);

    // Elite: is a roll lane covering `gemColumn` at `position` on this difficulty? Flams
    // inside a roll lane are ignored — the gem draws as a single note.
    static bool eliteRollLaneCovers(const SharedWindow& shared, uint gemColumn,
                                    PPQ position, SkillLevel skill);
};
