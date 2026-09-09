/*
    ==============================================================================

        AuthoringConfig.h
        Author: Noah Baxter

        Per-instrument write-mode data, the authoring twin of
        PositionConstants::RenderTypeConfig. Replaces the isDrums/isElite
        branches and hardcoded lane numbers that were spread across
        AuthoringControllerBase, WriteController and EditController with one
        const-pointer indirection: `cfg->X`.

        Adding an instrument should mean adding a table entry here, not hunting
        through Editor/ for every place that assumed four lanes.

    ==============================================================================
*/

#pragma once

#include "../Utils/ChartTypes.h"
#include "../Midi/Utils/InstrumentMapper.h"

// Per-instrument authoring data. Static-lifetime singletons returned by
// getAuthoringConfig(). Data and small pure functions only; genuine behaviour
// branches stay in the controllers.
struct AuthoringConfig
{
    // Highest authorable lane index. Elite's 2x kick is a virtual column ABOVE its hand
    // lanes (9), while 4-lane's sits inside the range (6), so both forms are stored.
    int highestLane;        // 2x kick disabled
    int highestLaneKick2x;  // 2x kick enabled

    // Lane the 2x kick lives on, or -1 where the instrument has none.
    int kick2xColumn;

    // Whether the Cym modifier means anything. Elite fixes cymbal-ness by lane, so its
    // toggle is inert and the write sub-toolbar omits the slot.
    bool hasCymbalToggle;

    // Lane the user clicked -> pitch to write. Handles this instrument's kicks itself, so
    // callers never need a kick special case. Returns -1 for an unauthorable lane.
    int (*laneToPitch)(SkillLevel skill, int lane, bool kick2xEnabled);

    // Kick exclusivity: whether `pitch` is one of this instrument's kicks at this skill,
    // and which kick it conflicts with. Both are no-ops where kick2xColumn is -1.
    bool (*isKickPitch)(uint pitch, SkillLevel skill);
    InstrumentMapper::KickConflict (*conflictingKick)(int pitch, SkillLevel skill);
};

// Returns nullptr for render types that aren't authorable. Callers are gated upstream by
// write mode only being offered for authorable parts.
const AuthoringConfig* getAuthoringConfig(RenderType type);

// Convenience for the common `getAuthoringConfig(getRenderType(part))` pair.
inline const AuthoringConfig* getAuthoringConfig(Part part)
{
    return getAuthoringConfig(getRenderType(part));
}
