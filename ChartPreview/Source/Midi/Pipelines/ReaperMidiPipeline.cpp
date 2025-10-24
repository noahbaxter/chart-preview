/*
  ==============================================================================

    ReaperMidiPipeline.cpp
    REAPER timeline MIDI processing pipeline with caching

  ==============================================================================
*/

#include "ReaperMidiPipeline.h"
#include "../../REAPER/ReaperTrackDetector.h"

ReaperMidiPipeline::ReaperMidiPipeline(MidiProcessor& processor,
                                       ReaperMidiProvider& provider,
                                       juce::ValueTree& pluginState,
                                       std::function<void(const juce::String&)> printFunc)
    : midiProcessor(processor),
      reaperProvider(provider),
      state(pluginState),
      print(printFunc)
{
}

void ReaperMidiPipeline::process(const juce::AudioPlayHead::PositionInfo& position,
                                 uint blockSize,
                                 double sampleRate)
{
    if (!reaperProvider.isReaperApiAvailable())
        return;

    PPQ newPosition = position.getPpqPosition().orFallback(0.0);
    bool nowPlaying = position.getIsPlaying();
    double bpm = position.getBpm().orFallback(120.0);

    // Update current state
    currentPosition = newPosition;
    playing = nowPlaying;

    // Refetch MIDI data whenever it changes (detected via hash)
    if (checkMidiHashChanged())
    {
        refetchAllMidiData();
    }

    // Process cached notes into noteStateMapArray
    processCachedNotesIntoState(currentPosition, bpm, sampleRate);
}

void ReaperMidiPipeline::setDisplayWindow(PPQ start, PPQ end)
{
    displayWindowStart = start;
    displayWindowEnd = end;
    displayWindowSize = end - start;
}

PPQ ReaperMidiPipeline::getCurrentPosition() const
{
    return currentPosition;
}

bool ReaperMidiPipeline::isPlaying() const
{
    return playing;
}

void ReaperMidiPipeline::refetchAllMidiData()
{
    // Clear all old note and tempo data from the MidiProcessor
    {
        const juce::ScopedLock lock(midiProcessor.noteStateMapLock);
        for (auto& noteStateMap : midiProcessor.noteStateMapArray)
        {
            noteStateMap.clear();
        }
    }

    {
        const juce::ScopedLock lock(midiProcessor.tempoTimeSignatureMapLock);
        midiProcessor.tempoTimeSignatureMap.clear();
    }

    // Fetch all midi events from the session (one-time bulk fetch)
    fetchAllNoteEvents();
    fetchAllTempoTimeSignatureEvents();

    // Process newly fetched notes into state immediately
    // Default bpm/sampleRate will be updated on next process() call
    processCachedNotesIntoState(currentPosition, 120.0, 48000.0);
}

void ReaperMidiPipeline::fetchAllNoteEvents()
{
    int configuredTrackIndex = (int)state.getProperty("reaperTrack") - 1;
    auto notes = reaperProvider.getAllNotesFromTrack(configuredTrackIndex);

    // Store basic midi note info in midiCache's allNotes
    allNotes.clear();
    for (const auto& reaperNote : notes)
    {
        MidiCache::CachedNote cachedNote;
        cachedNote.startPPQ = PPQ(reaperNote.startPPQ);
        cachedNote.endPPQ = PPQ(reaperNote.endPPQ);
        cachedNote.pitch = reaperNote.pitch;
        cachedNote.velocity = reaperNote.velocity;
        cachedNote.channel = reaperNote.channel;
        cachedNote.muted = reaperNote.muted;
        allNotes.push_back(cachedNote);
    }
}

void ReaperMidiPipeline::fetchAllTempoTimeSignatureEvents()
{
    auto events = reaperProvider.getAllTempoTimeSignatureEvents();

    // Store in MidiProcessor's tempoTimeSignatureMap
    const juce::ScopedLock lock(midiProcessor.tempoTimeSignatureMapLock);
    midiProcessor.tempoTimeSignatureMap.clear();
    for (const auto& event : events)
    {
        midiProcessor.tempoTimeSignatureMap[event.ppqPosition] = event;
    }
}

void ReaperMidiPipeline::processCachedNotesIntoState(PPQ currentPos, double bpm, double sampleRate)
{
    // Get notes within current display window from persistent allNotes (not from windowed cache)
    // This ensures we use the same PPQ derivation as gridlines (bulk fetched with consistent REAPER state)
    PPQ clearStart = currentPos - PPQ(PREFETCH_BEHIND);
    PPQ clearEnd = currentPos + displayWindowSize + PPQ(PREFETCH_AHEAD);

    // Filter allNotes to get only those in the current window
    std::vector<MidiCache::CachedNote> visibleNotes;
    for (const auto& note : allNotes)
    {
        // Include note if it overlaps with current window
        if (note.endPPQ > clearStart && note.startPPQ < clearEnd)
        {
            visibleNotes.push_back(note);
        }
    }

    // CRITICAL: Hold the lock for the ENTIRE clear+write operation!
    // This prevents race conditions where the renderer could read an empty noteStateMapArray
    // between the clear and write operations (which was causing intermittent black screens).
    {
        const juce::ScopedLock lock(midiProcessor.noteStateMapLock);

        // Clear the range
        for (auto& noteStateMap : midiProcessor.noteStateMapArray)
        {
            auto lower = noteStateMap.lower_bound(clearStart);
            auto upper = noteStateMap.upper_bound(clearEnd);
            noteStateMap.erase(lower, upper);
        }

        // Delegate note processing to specialized processor (holds lock internally)
        noteProcessor.processModifierNotes(visibleNotes, midiProcessor.noteStateMapArray, midiProcessor.noteStateMapLock, state);
        noteProcessor.processPlayableNotes(visibleNotes, midiProcessor.noteStateMapArray, midiProcessor.noteStateMapLock, midiProcessor, state, bpm, sampleRate);
    }
}

bool ReaperMidiPipeline::checkMidiHashChanged()
{
    int configuredTrackIndex = (int)state.getProperty("reaperTrack") - 1;
    int trackIndex = targetTrackIndex >= 0 ? targetTrackIndex : configuredTrackIndex;

    std::string currentHash = reaperProvider.getTrackHash(trackIndex, true);  // notesonly=true
    if (currentHash != previousMidiHash)
    {
        previousMidiHash = currentHash;
        return true;
    }

    return false;
}


