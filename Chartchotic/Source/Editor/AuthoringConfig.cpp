/*
    ==============================================================================

        AuthoringConfig.cpp
        Author: Noah Baxter

    ==============================================================================
*/

#include "AuthoringConfig.h"

namespace
{
    //==========================================================================
    // Guitar (5-fret): lane 0 is the open bar, 1-5 the frets. No kicks.

    int guitarLaneToPitch(SkillLevel skill, int lane, bool)
    {
        return InstrumentMapper::columnToGuitarPitch(skill, lane);
    }

    bool guitarNoKick(uint, SkillLevel) { return false; }

    InstrumentMapper::KickConflict guitarNoConflict(int, SkillLevel)
    {
        return { -1, -1 };
    }

    //==========================================================================
    // 4-lane drums: kick 0, pads 1-4, 2x kick on virtual column 6.

    int drumLaneToPitch(SkillLevel skill, int lane, bool kick2xEnabled)
    {
        if (InstrumentMapper::isKickLane(lane))
            return InstrumentMapper::resolveKickPitch(skill, lane, kick2xEnabled);
        return InstrumentMapper::columnToDrumPitch(skill, lane, false);
    }

    bool drumIsKickPitch(uint pitch, SkillLevel)
    {
        return InstrumentMapper::isDrumKick(pitch);
    }

    InstrumentMapper::KickConflict drumConflictingKick(int pitch, SkillLevel)
    {
        return InstrumentMapper::getConflictingKick(pitch);
    }

    //==========================================================================
    // Elite drums: kick 0, hand lanes 1-8, 2x kick on virtual column 9. Column 6 is
    // Tom 3, NOT a kick, which is the assumption that broke every 4-lane-shaped check.

    int eliteLaneToPitch(SkillLevel skill, int lane, bool kick2xEnabled)
    {
        if (lane == ELITE_KICK_2X_COLUMN && !kick2xEnabled) return -1;
        return InstrumentMapper::columnToEliteDrumPitch(skill, lane);
    }

    //==========================================================================

    const AuthoringConfig guitarFiveFret = {
        5, 5, -1, false,
        &guitarLaneToPitch,
        &guitarNoKick,
        &guitarNoConflict,
    };

    const AuthoringConfig drumsFourLane = {
        4, DRUM_KICK_2X_COLUMN, DRUM_KICK_2X_COLUMN, true,
        &drumLaneToPitch,
        &drumIsKickPitch,
        &drumConflictingKick,
    };

    const AuthoringConfig eliteDrums = {
        8, ELITE_KICK_2X_COLUMN, ELITE_KICK_2X_COLUMN, false,
        &eliteLaneToPitch,
        &InstrumentMapper::isEliteDrumKick,
        &InstrumentMapper::getConflictingEliteKick,
    };
}

const AuthoringConfig* getAuthoringConfig(RenderType type)
{
    switch (type)
    {
        case RenderType::FIVE_FRET:       return &guitarFiveFret;
        case RenderType::FOUR_LANE_DRUMS: return &drumsFourLane;
        case RenderType::ELITE_DRUMS:     return &eliteDrums;
        default:                          return nullptr;
    }
}
