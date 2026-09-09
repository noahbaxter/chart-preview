#pragma once

#include <JuceHeader.h>
#include "../Utils/ChartTypes.h"
#include "../Midi/Utils/PPQ.h"
#include "../Midi/Utils/TimeConverter.h"
#include "../Visual/HighwayComponent.h"
#include "../Visual/HighwaySlot.h"
#include "../Visual/Managers/GridlineGenerator.h"
#include "../Midi/Processing/MidiInterpreter.h"

class ChartchoticAudioProcessor;

// Snapshot of per-frame state needed by all frame data builders.
// Filled once by PluginEditor::onFrame(), passed to static build methods.
struct FrameContext
{
    ChartchoticAudioProcessor& processor;
    juce::ValueTree& state;

    PPQ cursorPPQ;              // Playhead position (latency-offset applied)
    PPQ displaySizeInPPQ;
    double displayWindowTimeSeconds;
    float scrollOffset;
    PPQ smoothedLatencyInPPQ;   // Only used by standard mode

    // Slot access (for batched mode)
    HighwaySlot* slots;
    int activeSlotCount;

    // Write-mode STEP gridline overlay config (defaults to inactive)
    WriteGridConfig writeGridConfig;
};

class FrameDataBuilder
{
public:
    // Single-slot REAPER mode
    static void buildReaper(HighwayFrameData& out,
                            const FrameContext& ctx,
                            MidiInterpreter& interpreter);

    // Multi-slot REAPER mode (batched: extract once, resolve once per lock group)
    static void buildReaperBatched(HighwayFrameData& primaryOut,
                                   const FrameContext& ctx);

    // Standard (non-REAPER) mode
    static void buildStandard(HighwayFrameData& out,
                              const FrameContext& ctx,
                              MidiInterpreter& interpreter);
};
