/*
  ==============================================================================

    LaneDetector.h
    Created: 18 Oct 2024
    Author:  Noah Baxter

    Detects and generates lane sustain events from lane modifier pitches.
    Handles both skill-level-specific lane application and 1-lane vs 2-lane variants.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "MidiTypes.h"
#include "../../Utils/Utils.h"

class LaneDetector
{
public:
    static std::vector<SustainEvent> detectLanes(uint laneType, PPQ startPPQ, PPQ endPPQ,
                                                  uint laneVelocity, juce::ValueTree& state,
                                                  NoteStateMapArray& noteStateMapArray,
                                                  juce::CriticalSection& noteStateMapLock);
};
