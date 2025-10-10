/*
  ==============================================================================

    MidiCache.cpp
    Cache for REAPER timeline MIDI data to prevent redundant fetches

  ==============================================================================
*/

#include "MidiCache.h"

void MidiCache::addNotes(const std::vector<ReaperMidiProvider::ReaperMidiNote>& notes, PPQ rangeStart, PPQ rangeEnd)
{
    const juce::ScopedLock lock(cacheLock);

    // Update cache range
    if (cacheStartPPQ < PPQ(0.0) || rangeStart < cacheStartPPQ)
        cacheStartPPQ = rangeStart;
    if (cacheEndPPQ < PPQ(0.0) || rangeEnd > cacheEndPPQ)
        cacheEndPPQ = rangeEnd;

    for (const auto& note : notes)
    {
        PPQ noteStart(note.startPPQ);
        PPQ noteEnd(note.endPPQ);

        // Check if note already exists in cache
        bool exists = std::any_of(cache.begin(), cache.end(),
            [&](const CachedNote& cn) {
                return cn.startPPQ == noteStart &&
                       cn.pitch == note.pitch &&
                       cn.channel == note.channel;
            });

        if (!exists)
        {
            cache.push_back({
                noteStart,
                noteEnd,
                static_cast<uint>(note.pitch),
                static_cast<uint>(note.velocity),
                static_cast<uint>(note.channel),
                note.muted,
                false  // not processed yet
            });
        }
    }

    // Sort cache by time for efficient retrieval
    std::sort(cache.begin(), cache.end(),
        [](const CachedNote& a, const CachedNote& b) {
            if (a.startPPQ != b.startPPQ)
                return a.startPPQ < b.startPPQ;
            return a.pitch < b.pitch;  // Secondary sort by pitch for deterministic order
        });
}

std::vector<MidiCache::CachedNote> MidiCache::getNotesInRange(PPQ start, PPQ end) const
{
    const juce::ScopedLock lock(cacheLock);
    std::vector<CachedNote> result;
    result.reserve(cache.size() / 4); // Estimate to reduce reallocations

    for (const auto& note : cache)
    {
        // Include notes that overlap with the range
        if (note.endPPQ >= start && note.startPPQ <= end)
        {
            result.push_back(note);
        }
        // Can break early if notes are sorted and we've passed the range
        else if (note.startPPQ > end)
        {
            break;
        }
    }

    return result;
}

void MidiCache::cleanup(PPQ beforePosition)
{
    const juce::ScopedLock lock(cacheLock);

    // Remove notes that end before the cleanup position
    cache.erase(
        std::remove_if(cache.begin(), cache.end(),
            [beforePosition](const CachedNote& note) {
                return note.endPPQ < beforePosition;
            }),
        cache.end()
    );

    // Update cache start if we've cleaned up the beginning
    if (!cache.empty())
    {
        cacheStartPPQ = cache.front().startPPQ;
    }
    else
    {
        cacheStartPPQ = PPQ(-1.0);
        cacheEndPPQ = PPQ(-1.0);
    }
}

void MidiCache::clear()
{
    const juce::ScopedLock lock(cacheLock);
    cache.clear();
    cacheStartPPQ = PPQ(-1.0);
    cacheEndPPQ = PPQ(-1.0);
}

bool MidiCache::hasDataForRange(PPQ start, PPQ end) const
{
    const juce::ScopedLock lock(cacheLock);

    // Check if we have valid cache
    if (cacheStartPPQ < PPQ(0.0) || cacheEndPPQ < PPQ(0.0))
        return false;

    // Check if requested range is fully within our cached range
    return start >= cacheStartPPQ && end <= cacheEndPPQ;
}