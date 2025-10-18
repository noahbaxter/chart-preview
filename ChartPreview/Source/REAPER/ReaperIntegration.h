/*
  ==============================================================================

    ReaperIntegration.h
    REAPER timeline MIDI processing

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Utils/PPQ.h"
#include "../Midi/Utils/InstrumentMapper.h"

class ChartPreviewAudioProcessor;

class ReaperIntegration
{
public:
    static void processReaperTimelineMidi(
        ChartPreviewAudioProcessor& processor,
        PPQ startPPQ,
        PPQ endPPQ,
        double bpm,
        juce::uint32 timeSignatureNumerator,
        juce::uint32 timeSignatureDenominator
    );
};