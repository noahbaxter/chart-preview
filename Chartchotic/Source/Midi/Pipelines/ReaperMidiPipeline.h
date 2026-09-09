/*
  ==============================================================================

    ReaperMidiPipeline.h
    REAPER timeline MIDI processing pipeline with caching

  ==============================================================================
*/

#pragma once

#include "MidiPipeline.h"
#include "../MidiProject.h"
#include "../Processing/NoteProcessor.h"
#include "../Providers/REAPER/MidiCache.h"
#include "../Providers/REAPER/ReaperMidiProvider.h"
#include "../DiscoFlipState.h"
#include "../../Utils/ChartTypes.h"

class ReaperMidiPipeline : public MidiPipeline
{
public:
    ReaperMidiPipeline(MidiProject& project,
                      ReaperMidiProvider& provider,
                      juce::ValueTree& pluginState,
                      std::function<void(const juce::String&)> printFunc = nullptr);

    void process(const juce::AudioPlayHead::PositionInfo& position,
                uint blockSize,
                double sampleRate) override;

    bool needsRealtimeMidiBuffer() const override { return false; }

    void setDisplayWindow(PPQ start, PPQ end) override;

    PPQ getCurrentPosition() const override;
    bool isPlaying() const override;

    void setTargetTrackIndex(int trackIndex) { targetTrackIndex = trackIndex; }
    int getTargetTrackIndex() const { return targetTrackIndex; }

    void refetchAllMidiData();

    // Main-thread only. Checks track hash; if changed, refetches.
    // Replaces the audio-thread hash check that was removed from process().
    void pollForHashChange();

    void fetchAllNoteEvents();
    void fetchAllTempoTimeSignatureEvents();
    void fetchAllTextEvents();

    const DiscoFlipState* getDiscoFlipState() const { return discoFlipState.hasRegions() ? &discoFlipState : nullptr; }

private:
    void processCachedNotesIntoState(PPQ currentPos);
    bool checkMidiHashChanged();

    MidiProject& midiProject;
    ReaperMidiProvider& reaperProvider;
    juce::ValueTree& state;
    std::function<void(const juce::String&)> print;

    NoteProcessor noteProcessor;
    std::string previousMidiHash = "__uninitialized__";

    std::vector<MidiCache::CachedNote> allNotes;
    TrackTextEvents textEvents;
    DiscoFlipState discoFlipState;

    int targetTrackIndex = -1;

    PPQ displayWindowStart{0.0};
    PPQ displayWindowEnd{4.0};
    PPQ displayWindowSize{4.0};

    PPQ currentPosition{0.0};
    bool playing = false;

    static constexpr double PREFETCH_AHEAD = 8.0;
    static constexpr double PREFETCH_BEHIND = 1.0;
    static constexpr double MAX_HIGHWAY_LENGTH = 16.0;
};
