/*
  ==============================================================================

    ReaperMidiPipeline.cpp
    REAPER timeline MIDI processing pipeline with caching

  ==============================================================================
*/

#include "ReaperMidiPipeline.h"
#include "../../Host/ReaperTrackDetector.h"

ReaperMidiPipeline::ReaperMidiPipeline(MidiProject& project,
                                       ReaperMidiProvider& provider,
                                       juce::ValueTree& pluginState,
                                       std::function<void(const juce::String&)> printFunc)
    : midiProject(project),
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

    currentPosition = newPosition;
    playing = nowPlaying;

    // Audio thread: touch ONLY cached data. Hash-check + refetch moved to
    // pollForHashChange() driven by the editor's VBlank. Calling REAPER API
    // from here could deadlock when REAPER holds project locks (e.g. during
    // track duplication).
    processCachedNotesIntoState(currentPosition);
}

void ReaperMidiPipeline::pollForHashChange()
{
    if (!reaperProvider.isReaperApiAvailable())
        return;

    if (checkMidiHashChanged())
        refetchAllMidiData();
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
    auto& track = midiProject.primaryTrack();
    {
        const juce::ScopedLock lock(track.notesLock);
        for (auto& noteStateMap : track.notes)
            noteStateMap.clear();
    }

    {
        const juce::ScopedLock lock(midiProject.tempoLock);
        midiProject.tempoTimeSignatures.clear();
    }

    fetchAllNoteEvents();
    fetchAllTempoTimeSignatureEvents();
    fetchAllTextEvents();

    processCachedNotesIntoState(currentPosition);
}

void ReaperMidiPipeline::fetchAllNoteEvents()
{
    int configuredTrackIndex = (int)state.getProperty("reaperTrack") - 1;
    auto notes = reaperProvider.getAllNotesFromTrack(configuredTrackIndex);

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

    const juce::ScopedLock lock(midiProject.tempoLock);
    midiProject.tempoTimeSignatures.clear();
    for (const auto& event : events)
    {
        midiProject.tempoTimeSignatures[event.ppqPosition] = event;
    }
}

void ReaperMidiPipeline::fetchAllTextEvents()
{
    int configuredTrackIndex = (int)state.getProperty("reaperTrack") - 1;
    textEvents = reaperProvider.getAllTextEventsFromTrack(configuredTrackIndex);
    discoFlipState.buildFromTextEvents(textEvents);
    strictHatPedalState.buildFromTextEvents(textEvents);
}

void ReaperMidiPipeline::processCachedNotesIntoState(PPQ currentPos)
{
    auto& track = midiProject.primaryTrack();

    PPQ clearStart = currentPos - PPQ(PREFETCH_BEHIND);
    PPQ clearEnd = currentPos + displayWindowSize + PPQ(PREFETCH_AHEAD);

    std::vector<MidiCache::CachedNote> visibleNotes;
    for (const auto& note : allNotes)
    {
        if (note.endPPQ > clearStart && note.startPPQ < clearEnd)
        {
            visibleNotes.push_back(note);
        }
    }

    // CRITICAL: Hold the lock for the ENTIRE clear+write operation
    {
        const juce::ScopedLock lock(track.notesLock);

        for (auto& noteStateMap : track.notes)
        {
            auto lower = noteStateMap.lower_bound(clearStart);
            auto upper = noteStateMap.upper_bound(clearEnd);
            noteStateMap.erase(lower, upper);
        }

        noteProcessor.processAllNotes(visibleNotes, track.notes);
    }
}

bool ReaperMidiPipeline::checkMidiHashChanged()
{
    int configuredTrackIndex = (int)state.getProperty("reaperTrack") - 1;
    int trackIndex = targetTrackIndex >= 0 ? targetTrackIndex : configuredTrackIndex;

    std::string currentHash = reaperProvider.getTrackHash(trackIndex, true);
    if (currentHash != previousMidiHash)
    {
        previousMidiHash = currentHash;
        return true;
    }

    return false;
}
