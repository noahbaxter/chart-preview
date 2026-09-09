#pragma once

#include "AuthoringTypes.h"
#include "../UI/ControlConstants.h"
#include <vector>

//==============================================================================
// Positional note-type modifiers.
//
// The home row (A S D F G) is slot 1..5 regardless of instrument, so the same
// finger means "modifier 2" whether you are charting drums or guitar. Each slot
// names a group and the value it sets. Pressing a slot that is already active
// returns its group to that group's default, which is why Normal/None need no
// key of their own.
//
// Groups are independent of each other: on drums, Ghost and Accent share the
// Dynamic group and so are mutually exclusive, while Cymbal is its own group
// and toggles freely alongside either.

enum class ModifierGroup
{
    Dynamic,   // DrumDynamic: Normal / Ghost / Accent
    Cymbal,    // bool
    Force,     // GuitarForce: None / Hopo / Strum / Tap
};

struct ModifierSlot
{
    ModifierGroup group;
    int           value;   // cast target depends on group
    const char*   label;   // shown in the sub-toolbar hover help
};

// Slots in home-row order, which MUST stay in the same order as the controls
// read left-to-right in the write sub-toolbar. Drums show [Normal|Ghost|Accent]
// then [Cym]; guitar shows [Auto|HOPO|Strum|Tap]. Slot 1 is the group default
// on both, so A always means "clear back to plain".
//
// An instrument with fewer than five slots returns a shorter list, and the
// leftover keys stay inert rather than doing something surprising.
inline std::vector<ModifierSlot> modifierSlotsFor(RenderType type)
{
    switch (type)
    {
        case RenderType::FOUR_LANE_DRUMS:
        case RenderType::FIVE_LANE_DRUMS:
            return {
                { ModifierGroup::Dynamic, (int)DrumDynamic::Normal, "normal" },
                { ModifierGroup::Dynamic, (int)DrumDynamic::Ghost,  "ghost"  },
                { ModifierGroup::Dynamic, (int)DrumDynamic::Accent, "accent" },
                { ModifierGroup::Cymbal,  1,                        "cymbal" },
            };

        // Elite lanes are drum XOR cymbal, fixed by lane (TrackResolver decides it from
        // isEliteCymbalLane), so a Cymbal toggle would be inert. Dynamics still come from
        // velocity exactly as 4-lane, so those three slots carry over.
        case RenderType::ELITE_DRUMS:
            return {
                { ModifierGroup::Dynamic, (int)DrumDynamic::Normal, "normal" },
                { ModifierGroup::Dynamic, (int)DrumDynamic::Ghost,  "ghost"  },
                { ModifierGroup::Dynamic, (int)DrumDynamic::Accent, "accent" },
            };

        case RenderType::FIVE_FRET:
        case RenderType::SIX_FRET:
            return {
                { ModifierGroup::Force, (int)GuitarForce::None,  "auto"  },
                { ModifierGroup::Force, (int)GuitarForce::Hopo,  "HOPO"  },
                { ModifierGroup::Force, (int)GuitarForce::Strum, "strum" },
                { ModifierGroup::Force, (int)GuitarForce::Tap,   "tap"   },
            };

        // Nothing authorable yet; no slots rather than wrong slots.
        case RenderType::VOCALS:
        case RenderType::PRO_GUITAR:
        case RenderType::PRO_KEYS:
        default:
            return {};
    }
}
