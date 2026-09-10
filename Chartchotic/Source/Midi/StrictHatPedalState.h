#pragma once

#include "ChartTextEvent.h"

/**
 * The [STRICT_HAT_PEDAL_STATE] text event on an elite drums track. Latched and chart-global:
 * it applies from its position onward and cannot be disabled, so the earliest occurrence is
 * the whole state. Under it a Yellow Cymbal stops suppressing a coincident Stomp/Splash.
 */
class StrictHatPedalState
{
public:
    StrictHatPedalState() = default;

    void buildFromTextEvents(const TrackTextEvents& events)
    {
        strictFromPPQ = PPQ(NEVER);
        for (const auto& evt : events)
        {
            if (evt.text.trim() == "[STRICT_HAT_PEDAL_STATE]" && evt.position < strictFromPPQ)
                strictFromPPQ = evt.position;
        }
    }

    bool isStrictAt(PPQ position) const { return position >= strictFromPPQ; }
    bool hasFlag() const { return strictFromPPQ < PPQ(NEVER); }

private:
    static constexpr double NEVER = 1e12;
    PPQ strictFromPPQ { NEVER };
};
