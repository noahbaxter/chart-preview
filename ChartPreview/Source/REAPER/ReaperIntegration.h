/*
  ==============================================================================

    ReaperIntegration.h
    REAPER timeline MIDI processing

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Utils/PPQ.h"

class ChartPreviewAudioProcessor;

class ReaperIntegration
{
public:
    static void processReaperTimelineMidi(
        ChartPreviewAudioProcessor& processor,
        PPQ startPPQ,
        PPQ endPPQ,
        double bpm,
        uint timeSignatureNumerator,
        uint timeSignatureDenominator
    );
};