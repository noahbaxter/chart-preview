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

    // Check if playing state changed
    bool playStateChanged = (nowPlaying != playing);

    // Check if position jumped significantly (scrubbing when paused OR timeline jump during playback)
    double positionDelta = std::abs((newPosition - currentPosition).toDouble());
    bool positionChanged = positionDelta > 1.0;

    // Update current state
    currentPosition = newPosition;
    playing = nowPlaying;

    // Only invalidate cache when:
    // 1. MIDI actually changed (detected via hash, only checked when paused)
    // 2. Position moved enough that we need more data to display
    // Only check hash when paused - avoid constant polling while playing
    bool midiChanged = !nowPlaying && checkMidiHashChanged();
    if (midiChanged || positionChanged)
    {
        invalidateCache();
    }

    // Calculate fetch window (larger than display for smooth scrolling)
    PPQ fetchWindowStart = currentPosition - PPQ(PREFETCH_BEHIND);
    PPQ fetchWindowEnd = currentPosition + displayWindowSize + PPQ(PREFETCH_AHEAD);

    // PLAYING MODE: Use conservative fetch threshold to avoid constant re-fetching
    // Only fetch if we've moved significantly beyond tolerance OR never fetched OR forced
    double fetchThreshold = playing ? FETCH_TOLERANCE_PLAYING : FETCH_TOLERANCE_PAUSED;
    bool needsFetch = forceNextFetch ||
                     (lastFetchedStart < PPQ(0.0)) ||
                     (fetchWindowStart < lastFetchedStart - PPQ(fetchThreshold)) ||
                     (fetchWindowEnd > lastFetchedEnd + PPQ(fetchThreshold));

    if (needsFetch)
    {
        // Clear the force flag after using it
        forceNextFetch = false;

        // Log fetch behavior (useful for testing)
        static PPQ lastLoggedFetchPos = PPQ(-100.0);
        if (print && std::abs((currentPosition - lastLoggedFetchPos).toDouble()) > 1.0)
        {
            print("📦 " + juce::String(playing ? "Playing" : "Paused") +
                  " - fetching range [" + juce::String(fetchWindowStart.toDouble(), 2) +
                  " to " + juce::String(fetchWindowEnd.toDouble(), 2) + "]");
            lastLoggedFetchPos = currentPosition;
        }
        fetchTimelineData(fetchWindowStart, fetchWindowEnd);
    }

    // Process cached notes into noteStateMapArray
    processCachedNotesIntoState(currentPosition, bpm, sampleRate);

    // Clean up old data (only keep what's needed for current visual window)
    // This ensures no data accumulation beyond the visible range
    PPQ cleanupStart = currentPosition - PPQ(PREFETCH_BEHIND);
    PPQ cleanupEnd = currentPosition + displayWindowSize + PPQ(PREFETCH_AHEAD);
    cache.cleanupOutsideRange(cleanupStart, cleanupEnd);
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

void ReaperMidiPipeline::invalidateCache()
{
    cache.clear();
    lastFetchedStart = PPQ(-1000.0);
    lastFetchedEnd = PPQ(-1000.0);
    forceNextFetch = true;  // Backup flag in case immediate fetch fails

    // Also clear all note data from the MidiProcessor so old notes don't linger
    {
        const juce::ScopedLock lock(midiProcessor.noteStateMapLock);
        for (auto& noteStateMap : midiProcessor.noteStateMapArray)
        {
            noteStateMap.clear();
        }
    }

    // Clear tempo/timesig map too
    {
        const juce::ScopedLock lock(midiProcessor.tempoTimeSignatureMapLock);
        midiProcessor.tempoTimeSignatureMap.clear();
    }

    // IMMEDIATELY fetch all tempo/timesig events (one-time bulk fetch for the entire session)
    // This is more efficient than fetching per-frame
    fetchAllTempoTimeSignatureEvents();

    // IMMEDIATELY fetch MIDI data for current position instead of waiting for next process() call
    // This is crucial when paused, as processBlock might not be called frequently
    PPQ fetchWindowStart = currentPosition - PPQ(PREFETCH_BEHIND);
    PPQ fetchWindowEnd = currentPosition + displayWindowSize + PPQ(PREFETCH_AHEAD);

    fetchTimelineData(fetchWindowStart, fetchWindowEnd);

    // Process the newly fetched data into state immediately
    // Use a reasonable default BPM if we don't have current tempo info
    double bpm = 120.0;  // Default, will be updated on next process() call
    double sampleRate = 48000.0;  // Default, will be updated on next process() call

    processCachedNotesIntoState(currentPosition, bpm, sampleRate);
}

void ReaperMidiPipeline::fetchTimelineData(PPQ start, PPQ end)
{
    // Use configured track or auto-detect
    int trackIdx = targetTrackIndex;

    // If not configured, try to auto-detect
    if (trackIdx < 0)
    {
        trackIdx = ReaperTrackDetector::detectPluginTrack(reaperProvider.getReaperGetFunc());

        // If still not found, default to track 0
        if (trackIdx < 0)
        {
            trackIdx = 0;
            // print("Warning: Could not detect plugin track, using Track 1");
        }
        else
        {
            // print("Auto-detected plugin on track " + juce::String(trackIdx + 1));
        }
    }

    // Calculate what new range we need to fetch
    PPQ fetchStart = start;
    PPQ fetchEnd = end;

    // If we have cached data, only fetch the new parts
    if (lastFetchedStart >= PPQ(0.0) && lastFetchedEnd >= PPQ(0.0))
    {
        // We might need to fetch before our current cache
        if (start < lastFetchedStart)
        {
            fetchEnd = lastFetchedStart;
        }
        // Or after our current cache
        else if (end > lastFetchedEnd)
        {
            fetchStart = lastFetchedEnd;
        }
        else
        {
            // We already have all the data we need
            return;
        }
    }

    // Debug logging
    bool shouldLog = (fetchStart != lastLoggedStartPPQ || fetchEnd != lastLoggedEndPPQ);
    if (shouldLog)
    {
        // print("=== REAPER Pipeline Fetch ===");
        // print("Fetching from " + juce::String(fetchStart.toDouble(), 2) +
        //       " to " + juce::String(fetchEnd.toDouble(), 2));
        // print("Track: " + juce::String(trackIdx));
        lastLoggedStartPPQ = fetchStart;
        lastLoggedEndPPQ = fetchEnd;
    }

    // Get configured track index from state (0-indexed, menu is 1-indexed)
    int configuredTrackIndex = (int)state.getProperty("reaperTrack") - 1;

    // Fetch notes from REAPER with retry logic for stability
    auto notes = reaperProvider.getNotesInRange(
        fetchStart.toDouble(),
        fetchEnd.toDouble(),
        configuredTrackIndex
    );

    // Handle empty fetch results with consistency checking
    if (notes.empty())
    {
        // Check if this is the same range as our last empty fetch
        bool sameRange = (fetchStart == lastEmptyFetchStart && fetchEnd == lastEmptyFetchEnd);

        if (sameRange)
        {
            consecutiveEmptyFetches++;
        }
        else
        {
            // Different range, reset counter
            consecutiveEmptyFetches = 1;
            lastEmptyFetchStart = fetchStart;
            lastEmptyFetchEnd = fetchEnd;
        }

        // If we have notes in cache but got 0 notes back, this might be a transient failure
        bool cacheHasNotes = cache.hasNotesInRange(fetchStart, fetchEnd);

        if (cacheHasNotes)
        {
            // Only trust the empty result after multiple consecutive confirmations
            if (consecutiveEmptyFetches < EMPTY_FETCH_CONFIRMATION_COUNT)
            {
                // Tiny delay to let REAPER finish any pending updates
                juce::Thread::sleep(2);

                notes = reaperProvider.getNotesInRange(
                    fetchStart.toDouble(),
                    fetchEnd.toDouble(),
                    configuredTrackIndex
                );

                if (!notes.empty())
                {
                    // Retry succeeded! Reset counter
                    consecutiveEmptyFetches = 0;
                }

                // Don't update cache with empty result yet - keep existing data
                return;
            }
            else
            {
                // Multiple consecutive empty fetches - trust it, notes were probably deleted
                // Fall through to update cache with empty result
            }
        }
    }
    else
    {
        // Got notes successfully - reset empty fetch counter
        consecutiveEmptyFetches = 0;
    }

    // Update cache with the fetched data (even if empty, after confirmation)
    cache.addNotes(notes, fetchStart, fetchEnd);

    // Update fetched range
    if (lastFetchedStart < PPQ(0.0) || fetchStart < lastFetchedStart)
        lastFetchedStart = fetchStart;
    if (lastFetchedEnd < PPQ(0.0) || fetchEnd > lastFetchedEnd)
        lastFetchedEnd = fetchEnd;
}

void ReaperMidiPipeline::fetchAllTempoTimeSignatureEvents()
{
    // Query REAPER for ALL tempo/timesig events in the session (one-time bulk fetch)
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
    // Get notes from cache for current window
    PPQ clearStart = currentPos - PPQ(PREFETCH_BEHIND);
    PPQ clearEnd = currentPos + displayWindowSize + PPQ(PREFETCH_AHEAD);
    auto cachedNotes = cache.getNotesInRange(clearStart, clearEnd);

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

        // Immediately write new data while still holding the lock
        processModifierNotes(cachedNotes);
        processPlayableNotes(cachedNotes, bpm, sampleRate);
    }

    // Note: Tempo/timesig events are fetched once via fetchAllTempoTimeSignatureEvents()
    // (called on session load or when markers change), not per-frame for efficiency.
    // The entire tempo map is cached in midiProcessor.tempoTimeSignatureMap.
    //
    // Cleanup is minimal: we only keep events within the extended window
    // (GridlineGenerator will use the full cached map for queries)
}

void ReaperMidiPipeline::processModifierNotes(const std::vector<MidiCache::CachedNote>& notes)
{
    SkillLevel currentSkill = (SkillLevel)((int)state.getProperty("skillLevel"));
    std::vector<uint> validModifierPitches;

    if (isPart(state, Part::DRUMS))
    {
        // Drum modifiers: tom markers (110-112), star power (116), lanes (126-127)
        validModifierPitches = {110, 111, 112, 116, 126, 127};
    }
    else if (isPart(state, Part::GUITAR))
    {
        // Guitar modifiers: HOPO/STRUM (per difficulty), TAP (104), star power (116), lanes (126-127)
        using Guitar = MidiPitchDefinitions::Guitar;
        switch (currentSkill)
        {
            case SkillLevel::EXPERT:
                validModifierPitches = {(uint)Guitar::EXPERT_HOPO, (uint)Guitar::EXPERT_STRUM,
                                      (uint)Guitar::TAP, (uint)Guitar::SP,
                                      (uint)Guitar::LANE_1, (uint)Guitar::LANE_2};
                break;
            case SkillLevel::HARD:
                validModifierPitches = {(uint)Guitar::HARD_HOPO, (uint)Guitar::HARD_STRUM,
                                      (uint)Guitar::TAP, (uint)Guitar::SP,
                                      (uint)Guitar::LANE_1, (uint)Guitar::LANE_2};
                break;
            case SkillLevel::MEDIUM:
                validModifierPitches = {(uint)Guitar::MEDIUM_HOPO, (uint)Guitar::MEDIUM_STRUM,
                                      (uint)Guitar::TAP, (uint)Guitar::SP,
                                      (uint)Guitar::LANE_1, (uint)Guitar::LANE_2};
                break;
            case SkillLevel::EASY:
                validModifierPitches = {(uint)Guitar::EASY_HOPO, (uint)Guitar::EASY_STRUM,
                                      (uint)Guitar::TAP, (uint)Guitar::SP,
                                      (uint)Guitar::LANE_1, (uint)Guitar::LANE_2};
                break;
        }
    }

    for (const auto& note : notes)
    {
        if (note.muted) continue;

        // Check if this is a valid modifier pitch
        bool isModifier = std::find(validModifierPitches.begin(), validModifierPitches.end(),
                                   note.pitch) != validModifierPitches.end();
        if (!isModifier) continue;

        // Add modifier to note state map (no gem type needed for modifiers)
        // NOTE: Caller must hold noteStateMapLock
        midiProcessor.noteStateMapArray[note.pitch][note.startPPQ] = NoteData(note.velocity, Gem::NONE);
        midiProcessor.noteStateMapArray[note.pitch][note.endPPQ - PPQ(1)] = NoteData(0, Gem::NONE);
    }
}

void ReaperMidiPipeline::processPlayableNotes(const std::vector<MidiCache::CachedNote>& notes,
                                              double bpm, double sampleRate)
{
    SkillLevel currentSkill = (SkillLevel)((int)state.getProperty("skillLevel"));
    std::vector<uint> validPlayablePitches;

    if (isPart(state, Part::DRUMS))
    {
        validPlayablePitches = InstrumentMapper::getDrumPitchesForSkill(currentSkill);
    }
    else if (isPart(state, Part::GUITAR))
    {
        validPlayablePitches = InstrumentMapper::getGuitarPitchesForSkill(currentSkill);
    }

    // Track guitar note positions for chord fixing (set automatically handles uniqueness)
    std::set<PPQ> guitarNotePositions;

    for (const auto& note : notes)
    {
        if (note.muted) continue;

        // Check if this is a valid playable pitch for current skill level
        bool isValidPlayablePitch = std::find(validPlayablePitches.begin(), validPlayablePitches.end(),
                                             note.pitch) != validPlayablePitches.end();
        if (!isValidPlayablePitch) continue;

        // Calculate gem type
        Gem gemType = Gem::NONE;
        if (note.velocity > 0)
        {
            if (isPart(state, Part::GUITAR))
            {
                gemType = midiProcessor.getGuitarGemType(note.pitch, note.startPPQ);
                guitarNotePositions.insert(note.startPPQ);
            }
            else if (isPart(state, Part::DRUMS))
            {
                Dynamic dynamic = (Dynamic)note.velocity;
                gemType = midiProcessor.getDrumGemType(note.pitch, note.startPPQ, dynamic);
            }
        }

        // Add to note state map
        // NOTE: Caller must hold noteStateMapLock
        midiProcessor.noteStateMapArray[note.pitch][note.startPPQ] = NoteData(note.velocity, gemType);
        midiProcessor.noteStateMapArray[note.pitch][note.endPPQ - PPQ(1)] = NoteData(0, Gem::NONE);
    }

    // Fix chord HOPOs for guitar (after all notes are added)
    if (isPart(state, Part::GUITAR) && !guitarNotePositions.empty())
    {
        std::vector<PPQ> positions(guitarNotePositions.begin(), guitarNotePositions.end());
        ChordAnalyzer::fixChordHOPOs(positions, currentSkill,
                                     midiProcessor.noteStateMapArray,
                                     midiProcessor.noteStateMapLock);
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


