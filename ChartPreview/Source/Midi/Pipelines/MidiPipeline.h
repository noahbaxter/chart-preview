/*
  ==============================================================================

    MidiPipeline.h
    Abstract interface for MIDI processing pipelines

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Utils/PPQ.h"

class MidiPipeline
{
public:
    virtual ~MidiPipeline() = default;

    // Main processing function - called from audio thread
    virtual void process(const juce::AudioPlayHead::PositionInfo& position,
                        juce::uint32 blockSize,
                        double sampleRate) = 0;

    // Indicates if this pipeline needs the realtime MIDI buffer
    // StandardMidiPipeline returns true (uses MIDI from DAW)
    // ReaperMidiPipeline returns false (uses timeline data)
    virtual bool needsRealtimeMidiBuffer() const = 0;

    // Set the display window for the GUI
    // This tells the pipeline what range of data to prepare
    virtual void setDisplayWindow(PPQ start, PPQ end) = 0;

    // Process the realtime MIDI buffer (only for StandardMidiPipeline)
    virtual void processMidiBuffer(juce::MidiBuffer& midiMessages,
                                  const juce::AudioPlayHead::PositionInfo& position,
                                  juce::uint32 blockSize,
                                  juce::uint32 latencySamples,
                                  double sampleRate) {}

    // Get the current playhead position
    virtual PPQ getCurrentPosition() const = 0;

    // Check if currently playing
    virtual bool isPlaying() const = 0;
};