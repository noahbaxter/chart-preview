/*
  ==============================================================================

    ReaperMidiPipeline.cpp
    REAPER timeline MIDI processing pipeline with caching

  ==============================================================================
*/

#include "ReaperMidiPipeline.h"
#include "../REAPER/ReaperTrackDetector.h"

ReaperMidiPipeline::ReaperMidiPipeline(MidiProcessor& processor,
                                       ReaperMidiProvider& provider,
                                       juce::ValueTree& pluginState)
    : midiProcessor(processor),
      reaperProvider(provider),
      state(pluginState)
{
}

void ReaperMidiPipeline::process(const juce::AudioPlayHead::PositionInfo& position,
                                 uint blockSize,
                                 double sampleRate)
{
    if (!reaperProvider.isReaperApiAvailable())
        return;

    currentPosition = position.getPpqPosition().orFallback(0.0);
    playing = position.getIsPlaying();
    double bpm = position.getBpm().orFallback(120.0);

    // Calculate fetch window (larger than display for smooth scrolling)
    PPQ fetchWindowStart = currentPosition - PPQ(PREFETCH_BEHIND);
    PPQ fetchWindowEnd = currentPosition + displayWindowSize + PPQ(PREFETCH_AHEAD);

    // Only fetch if we've moved beyond tolerance OR never fetched
    bool needsFetch = (lastFetchedStart < PPQ(0.0)) ||
                     (fetchWindowStart < lastFetchedStart - PPQ(FETCH_TOLERANCE)) ||
                     (fetchWindowEnd > lastFetchedEnd + PPQ(FETCH_TOLERANCE));

    if (needsFetch)
    {
        fetchTimelineData(fetchWindowStart, fetchWindowEnd);
    }

    // Process cached notes into noteStateMapArray
    processCachedNotesIntoState(currentPosition, bpm, sampleRate);

    // Clean up old data (keep some history for HOPO calculations)
    cache.cleanup(currentPosition - PPQ(PREFETCH_BEHIND * 2));
}

bool ReaperMidiPipeline::needsRealtimeMidiBuffer() const
{
    return false; // REAPER pipeline uses timeline data, not realtime MIDI
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

    // Get notes from REAPER
    // TODO: Add track index parameter to getNotesInRange
    auto notes = reaperProvider.getNotesInRange(
        fetchStart.toDouble(),
        fetchEnd.toDouble()
    );

    if (shouldLog)
    {
        // print("Fetched " + juce::String(notes.size()) + " notes");
    }

    // Add to cache (doesn't duplicate existing notes)
    cache.addNotes(notes, fetchStart, fetchEnd);

    // Update fetched range
    if (lastFetchedStart < PPQ(0.0) || fetchStart < lastFetchedStart)
        lastFetchedStart = fetchStart;
    if (lastFetchedEnd < PPQ(0.0) || fetchEnd > lastFetchedEnd)
        lastFetchedEnd = fetchEnd;
}

void ReaperMidiPipeline::processCachedNotesIntoState(PPQ currentPos, double bpm, double sampleRate)
{
    // Clear the visible range in noteStateMapArray
    PPQ clearStart = currentPos - PPQ(PREFETCH_BEHIND);
    PPQ clearEnd = currentPos + displayWindowSize + PPQ(PREFETCH_AHEAD);
    midiProcessor.clearNoteDataInRange(clearStart, clearEnd);

    // Get notes from cache for current window
    auto cachedNotes = cache.getNotesInRange(clearStart, clearEnd);

    // Process modifiers first (affects gem type calculation)
    processModifierNotes(cachedNotes);

    // Then process playable notes
    processPlayableNotes(cachedNotes, bpm, sampleRate);

    // Get time signature from position info (or default to 4/4)
    // Note: JUCE doesn't provide time signature directly, so we default to 4/4
    // In a full implementation, you might get this from REAPER API
    uint timeSignatureNumerator = 4;
    uint timeSignatureDenominator = 4;

    // Build gridlines for visible range
    buildGridlines(currentPos, currentPos + displayWindowSize,
                  timeSignatureNumerator, timeSignatureDenominator);
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
        {
            const juce::ScopedLock lock(midiProcessor.noteStateMapLock);
            midiProcessor.noteStateMapArray[note.pitch][note.startPPQ] = NoteData(note.velocity, Gem::NONE);
            midiProcessor.noteStateMapArray[note.pitch][note.endPPQ - PPQ(1)] = NoteData(0, Gem::NONE);
        }
    }
}

void ReaperMidiPipeline::processPlayableNotes(const std::vector<MidiCache::CachedNote>& notes,
                                              double bpm, double sampleRate)
{
    SkillLevel currentSkill = (SkillLevel)((int)state.getProperty("skillLevel"));
    std::vector<uint> validPlayablePitches;

    if (isPart(state, Part::DRUMS))
    {
        validPlayablePitches = MidiUtility::getDrumPitchesForSkill(currentSkill);
    }
    else if (isPart(state, Part::GUITAR))
    {
        validPlayablePitches = MidiUtility::getGuitarPitchesForSkill(currentSkill);
    }

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
            }
            else if (isPart(state, Part::DRUMS))
            {
                Dynamic dynamic = (Dynamic)note.velocity;
                gemType = midiProcessor.getDrumGemType(note.pitch, note.startPPQ, dynamic);
            }
        }

        // Add to note state map
        {
            const juce::ScopedLock lock(midiProcessor.noteStateMapLock);
            midiProcessor.noteStateMapArray[note.pitch][note.startPPQ] = NoteData(note.velocity, gemType);
            midiProcessor.noteStateMapArray[note.pitch][note.endPPQ - PPQ(1)] = NoteData(0, Gem::NONE);
        }
    }
}

void ReaperMidiPipeline::buildGridlines(PPQ startPPQ, PPQ endPPQ,
                                        uint timeSignatureNumerator,
                                        uint timeSignatureDenominator)
{
    // Find the nearest measure boundary before startPPQ to start gridline placement
    double measureLength = static_cast<double>(timeSignatureNumerator) * (4.0 / timeSignatureDenominator);
    double measureStart = std::floor(startPPQ.toDouble() / measureLength) * measureLength;
    PPQ gridlineStartPPQ = PPQ(measureStart);

    {
        const juce::ScopedLock lock(midiProcessor.gridlineMapLock);

        // Clear existing gridlines in range
        auto it = midiProcessor.gridlineMap.begin();
        while (it != midiProcessor.gridlineMap.end())
        {
            if (it->first >= startPPQ && it->first <= endPPQ)
            {
                it = midiProcessor.gridlineMap.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // Place gridlines for all half-beat boundaries in the visible range
        for (double ppq = gridlineStartPPQ.toDouble(); ppq <= endPPQ.toDouble(); ppq += 0.5)
        {
            PPQ gridlinePPQ = PPQ(ppq);

            // Skip if outside the actual range
            if (gridlinePPQ < startPPQ)
                continue;

            // Determine gridline type based on position relative to measure start
            double relativePosition = ppq - measureStart;
            if (std::abs(std::fmod(relativePosition, measureLength)) < 0.001)
            {
                midiProcessor.gridlineMap[gridlinePPQ] = Gridline::MEASURE;
            }
            else if (std::abs(std::fmod(relativePosition, 1.0)) < 0.001)
            {
                midiProcessor.gridlineMap[gridlinePPQ] = Gridline::BEAT;
            }
            else
            {
                midiProcessor.gridlineMap[gridlinePPQ] = Gridline::HALF_BEAT;
            }
        }
    }
}