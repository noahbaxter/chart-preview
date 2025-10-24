/*
  ==============================================================================

    ReaperMidiPipeline.h
    REAPER timeline MIDI processing pipeline with caching

  ==============================================================================
*/

#pragma once

#include "MidiPipeline.h"
#include "../Processing/MidiProcessor.h"
#include "../Processing/NoteProcessor.h"
#include "../Providers/REAPER/MidiCache.h"
#include "../Providers/REAPER/ReaperMidiProvider.h"
#include "../Utils/InstrumentMapper.h"
#include "../Utils/ChordAnalyzer.h"
#include "../../Utils/Utils.h"

class ReaperMidiPipeline : public MidiPipeline
{
public:
    ReaperMidiPipeline(MidiProcessor& processor,
                      ReaperMidiProvider& provider,
                      juce::ValueTree& pluginState,
                      std::function<void(const juce::String&)> printFunc = nullptr);

    void process(const juce::AudioPlayHead::PositionInfo& position,
                uint blockSize,
                double sampleRate) override;

    bool needsRealtimeMidiBuffer() const override { return false; }  // REAPER pipeline reads from timeline, not buffer

    void setDisplayWindow(PPQ start, PPQ end) override;

    PPQ getCurrentPosition() const override;
    bool isPlaying() const override;

    // Set the target track index (-1 for auto-detect)
    void setTargetTrackIndex(int trackIndex) { targetTrackIndex = trackIndex; }
    int getTargetTrackIndex() const { return targetTrackIndex; }

    // Fetch all note and tempo/timesig data from REAPER (call when track changes or settings change)
    void refetchAllMidiData();

    // Bulk fetch all MIDI events (faster than grabbing a smaller window)
    void fetchAllNoteEvents();
    void fetchAllTempoTimeSignatureEvents();

private:
    void processCachedNotesIntoState(PPQ currentPos, double bpm, double sampleRate);
    bool checkMidiHashChanged();

    MidiProcessor& midiProcessor;
    ReaperMidiProvider& reaperProvider;
    juce::ValueTree& state;
    std::function<void(const juce::String&)> print;

    NoteProcessor noteProcessor;
    std::string previousMidiHash; // Hash from last poll (for detecting if MIDI has changed)

    std::vector<MidiCache::CachedNote> allNotes;

    // Target track for MIDI data
    int targetTrackIndex = -1;  // -1 means auto-detect

    // Display window from GUI
    PPQ displayWindowStart{0.0};
    PPQ displayWindowEnd{4.0};
    PPQ displayWindowSize{4.0};

    // Current position tracking
    PPQ currentPosition{0.0};
    bool playing = false;

    // Filtering parameters (for per-frame window filtering of bulk-fetched data)
    static constexpr double PREFETCH_AHEAD = 8.0;            // Fetch 2 beats ahead (minimize data accumulation in REAPER mode)
    static constexpr double PREFETCH_BEHIND = 1.0;           // Keep 1 beat behind (minimize data accumulation in REAPER mode)
    static constexpr double MAX_HIGHWAY_LENGTH = 16.0;       // Maximum highway length in PPQ (16 beats = ~4 measures at 4/4)
};