/*
  ==============================================================================

    ReaperMidiPipeline.h
    REAPER timeline MIDI processing pipeline with caching

  ==============================================================================
*/

#pragma once

#include "MidiPipeline.h"
#include "../Midi/MidiProcessor.h"
#include "../Midi/MidiCache.h"
#include "../Midi/ReaperMidiProvider.h"
#include "../Utils/Utils.h"

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

    bool needsRealtimeMidiBuffer() const override;

    void setDisplayWindow(PPQ start, PPQ end) override;

    PPQ getCurrentPosition() const override;
    bool isPlaying() const override;

    // Set the target track index (-1 for auto-detect)
    void setTargetTrackIndex(int trackIndex) { targetTrackIndex = trackIndex; }
    int getTargetTrackIndex() const { return targetTrackIndex; }

    // Clear cache and force re-fetch (call when track changes or settings change)
    void invalidateCache();

private:
    void fetchTimelineData(PPQ start, PPQ end);
    void processCachedNotesIntoState(PPQ currentPos, double bpm, double sampleRate);
    void processModifierNotes(const std::vector<MidiCache::CachedNote>& notes);
    void processPlayableNotes(const std::vector<MidiCache::CachedNote>& notes, double bpm, double sampleRate);
    void buildGridlines(PPQ startPPQ, PPQ endPPQ, uint timeSignatureNumerator, uint timeSignatureDenominator);

    MidiProcessor& midiProcessor;
    ReaperMidiProvider& reaperProvider;
    juce::ValueTree& state;
    std::function<void(const juce::String&)> print;

    MidiCache cache;

    // Track fetched ranges
    PPQ lastFetchedStart{-1000.0};
    PPQ lastFetchedEnd{-1000.0};

    // Target track for MIDI data
    int targetTrackIndex = -1;  // -1 means auto-detect

    // Display window from GUI
    PPQ displayWindowStart{0.0};
    PPQ displayWindowEnd{4.0};
    PPQ displayWindowSize{4.0};

    // Current position tracking
    PPQ currentPosition{0.0};
    bool playing = false;
    bool forceNextFetch = false;  // Set by invalidateCache to force immediate fetch

    // Empty fetch tracking (to distinguish transient failures from legitimate empty sections)
    int consecutiveEmptyFetches = 0;
    PPQ lastEmptyFetchStart{-1000.0};
    PPQ lastEmptyFetchEnd{-1000.0};
    static constexpr int EMPTY_FETCH_CONFIRMATION_COUNT = 3;  // Need 3 consecutive empty fetches to believe it

    // Fetch parameters
    static constexpr double FETCH_TOLERANCE_PLAYING = 0.25;  // Re-fetch when moved 0.25 PPQ during playback
    static constexpr double FETCH_TOLERANCE_PAUSED = 0.1;    // More sensitive when paused (but invalidate is primary)
    static constexpr double PREFETCH_AHEAD = 8.0;            // Fetch 8 beats ahead (~2 measures)
    static constexpr double PREFETCH_BEHIND = 4.0;           // Keep 4 beats behind (~1 measure)

    // Debug logging control
    mutable PPQ lastLoggedStartPPQ{-1.0};
    mutable PPQ lastLoggedEndPPQ{-1.0};
};