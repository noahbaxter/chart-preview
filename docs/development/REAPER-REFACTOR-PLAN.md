# REAPER Integration Refactor Plan
**Date**: October 2025
**Status**: Planning Complete, Ready for Implementation

## Executive Summary

The current REAPER integration has critical architectural flaws:
1. MIDI processing happens in paint() at 60 FPS instead of audio thread
2. Data is cleared and refetched every frame (massive performance issue)
3. Hardcoded to Track 1 only
4. Files are too large (PluginProcessor.cpp is 689 lines)

This document outlines a complete refactor using Pipeline and Cache patterns.

## Current Issues

### 🔴 Critical Architecture Problems

1. **Wrong Thread**: MIDI processing in paint() instead of processBlock()
2. **No Caching**: Clears and refetches same data 60 times per second
3. **Wrong Track**: Hardcoded to Track 1, ignores plugin location
4. **Code Duplication**: 280+ lines duplicated between pipelines
5. **Monolithic Files**: PluginProcessor.cpp has VST2, VST3, and REAPER code mixed

### Current Broken Flow
```
paint() @ 60 FPS
  └── processReaperTimelineMidi()
      ├── clearNoteDataInRange() // Deletes everything!
      ├── getNotesInRange()       // Refetch same data
      └── 280 lines of processing // Every frame!
```

## Proposed Architecture

### Core Design Principles

1. **Pipeline Pattern**: Separate REAPER and standard VST logic completely
2. **Cache Pattern**: Only fetch new data when needed
3. **Modular Files**: No file over 200 lines
4. **Thread Safety**: Audio thread writes, GUI thread reads
5. **Track Detection**: Auto-detect or manually select track

### New File Structure

```
Source/
├── Core/
│   ├── PluginProcessor.h/cpp         (150 lines - VST basics only)
│   ├── PluginEditor.h/cpp            (200 lines - UI framework only)
│   └── PluginState.h/cpp             (State management)
│
├── Pipeline/
│   ├── MidiPipeline.h                 (Abstract interface)
│   ├── StandardMidiPipeline.h/cpp    (Non-REAPER DAWs)
│   ├── ReaperMidiPipeline.h/cpp      (REAPER timeline)
│   └── MidiPipelineFactory.h/cpp     (Creates correct pipeline)
│
├── REAPER/
│   ├── ReaperIntegration.h/cpp       (Main REAPER logic)
│   ├── ReaperMidiProvider.h/cpp      (Existing - API wrapper)
│   ├── ReaperTrackDetector.h/cpp     (Track detection)
│   ├── ReaperVST2Extensions.h/cpp    (VST2 extensions)
│   ├── ReaperVST3Extensions.h/cpp    (VST3 extensions)
│   └── ReaperVST3.h                  (Interface definitions)
│
├── Midi/
│   ├── MidiProcessor.h/cpp           (Standard processing)
│   ├── MidiCache.h/cpp               (Timeline caching)
│   └── MidiDataStructures.h          (Shared types)
│
└── UI/
    ├── EditorComponents.h/cpp        (Menu/control setup)
    ├── EditorCallbacks.h/cpp         (Event handlers)
    └── EditorTimer.h/cpp             (Timer & position tracking)
```

## Key Components

### 1. Pipeline Interface

```cpp
class MidiPipeline {
public:
    virtual void process(const juce::AudioPlayHead::PositionInfo& position,
                        uint blockSize, double sampleRate) = 0;
    virtual bool needsRealtimeMidiBuffer() const = 0;
    virtual void setDisplayWindow(PPQ start, PPQ end) = 0;
};
```

### 2. REAPER Pipeline with Caching (DETAILED)

```cpp
class ReaperMidiPipeline : public MidiPipeline {
private:
    ReaperMidiProvider& provider;
    MidiProcessor& midiProcessor;
    NoteStateMapArray& noteStateMapArray;
    juce::CriticalSection& noteStateMapLock;

    MidiCache cache;
    PPQ lastFetchedStart = -1000.0;
    PPQ lastFetchedEnd = -1000.0;
    int targetTrackIndex = -1;

    // Display window from GUI
    PPQ displayWindowStart = 0.0;
    PPQ displayWindowEnd = 4.0;
    PPQ displayWindowSize = 4.0;

    // Fetch tolerance to avoid micro-fetches
    const PPQ FETCH_TOLERANCE = 0.5;
    const PPQ PREFETCH_AHEAD = 8.0;  // Fetch 8 beats ahead
    const PPQ PREFETCH_BEHIND = 4.0; // Keep 4 beats behind

public:
    void process(const juce::AudioPlayHead::PositionInfo& position,
                uint blockSize, double sampleRate) override {

        PPQ currentPos = position.getPpqPosition().orFallback(0.0);
        double bpm = position.getBpm().orFallback(120.0);
        bool isPlaying = position.getIsPlaying();

        // Calculate fetch window (larger than display for smooth scrolling)
        PPQ fetchWindowStart = currentPos - PREFETCH_BEHIND;
        PPQ fetchWindowEnd = currentPos + displayWindowSize + PREFETCH_AHEAD;

        // Only fetch if we've moved beyond tolerance OR never fetched
        bool needsFetch = (lastFetchedStart < 0) ||
                         (fetchWindowStart < lastFetchedStart - FETCH_TOLERANCE) ||
                         (fetchWindowEnd > lastFetchedEnd + FETCH_TOLERANCE);

        if (needsFetch) {
            fetchTimelineData(fetchWindowStart, fetchWindowEnd);
        }

        // Process cached notes into noteStateMapArray
        processCachedNotesIntoState(currentPos, bpm, sampleRate);

        // Clean up old data (keep some history for HOPO calculations)
        cleanupOldData(currentPos - PREFETCH_BEHIND * 2);
    }

private:
    void fetchTimelineData(PPQ start, PPQ end) {
        // Detect or use configured track
        int trackIdx = (targetTrackIndex >= 0) ?
                      targetTrackIndex :
                      ReaperTrackDetector::detectPluginTrack(provider.getReaperApi);

        // Only fetch the NEW range we don't have
        PPQ fetchStart = (lastFetchedStart < 0) ? start :
                        std::min(start, lastFetchedStart);
        PPQ fetchEnd = (lastFetchedEnd < 0) ? end :
                      std::max(end, lastFetchedEnd);

        // Get notes from REAPER
        auto notes = provider.getNotesInRange(
            fetchStart.toDouble(),
            fetchEnd.toDouble(),
            trackIdx
        );

        // Add to cache (doesn't duplicate existing notes)
        cache.addNotes(notes, fetchStart, fetchEnd);

        // Update fetched range
        lastFetchedStart = fetchStart;
        lastFetchedEnd = fetchEnd;
    }

    void processCachedNotesIntoState(PPQ currentPos, double bpm, double sampleRate) {
        // Get notes from cache for current window
        auto cachedNotes = cache.getNotesInRange(
            currentPos - PREFETCH_BEHIND,
            currentPos + displayWindowSize + PREFETCH_AHEAD
        );

        // Process modifiers first (same as current implementation)
        processModifierNotes(cachedNotes);

        // Then process playable notes
        processPlayableNotes(cachedNotes, bpm, sampleRate);

        // Build gridlines for visible range
        buildGridlines(currentPos, currentPos + displayWindowSize);
    }

    void processModifierNotes(const std::vector<CachedNote>& notes) {
        // Filter and process HOPO/STRUM, TAP, tom markers, etc.
        // This is the fixed version of the current modifier processing
    }

    void processPlayableNotes(const std::vector<CachedNote>& notes,
                              double bpm, double sampleRate) {
        // Process actual playable notes with gem type calculation
        // Uses MidiUtility::get*PitchesForSkill() for proper filtering
    }
};
```

### 2b. MidiCache Implementation

```cpp
class MidiCache {
private:
    struct CachedNote {
        PPQ startPPQ;
        PPQ endPPQ;
        uint pitch;
        uint velocity;
        uint channel;
        bool processed = false;
    };

    std::vector<CachedNote> cache;
    mutable juce::CriticalSection cacheLock;

public:
    void addNotes(const std::vector<ReaperMidiNote>& notes,
                  PPQ rangeStart, PPQ rangeEnd) {
        const juce::ScopedLock lock(cacheLock);

        for (const auto& note : notes) {
            // Check if note already exists in cache
            bool exists = std::any_of(cache.begin(), cache.end(),
                [&](const CachedNote& cn) {
                    return cn.startPPQ == note.startPPQ &&
                           cn.pitch == note.pitch;
                });

            if (!exists) {
                cache.push_back({
                    PPQ(note.startPPQ),
                    PPQ(note.endPPQ),
                    note.pitch,
                    note.velocity,
                    note.channel,
                    false
                });
            }
        }

        // Sort cache by time for efficient retrieval
        std::sort(cache.begin(), cache.end(),
            [](const CachedNote& a, const CachedNote& b) {
                return a.startPPQ < b.startPPQ;
            });
    }

    std::vector<CachedNote> getNotesInRange(PPQ start, PPQ end) {
        const juce::ScopedLock lock(cacheLock);
        std::vector<CachedNote> result;

        for (const auto& note : cache) {
            if (note.endPPQ >= start && note.startPPQ <= end) {
                result.push_back(note);
            }
        }

        return result;
    }

    void cleanup(PPQ beforePosition) {
        const juce::ScopedLock lock(cacheLock);
        cache.erase(
            std::remove_if(cache.begin(), cache.end(),
                [beforePosition](const CachedNote& note) {
                    return note.endPPQ < beforePosition;
                }),
            cache.end()
        );
    }
};
```

### 3. Track Detection

```cpp
class ReaperTrackDetector {
    static int detectPluginTrack() {
        // Iterate through tracks
        // Find which has "Chart Preview" FX
        // Return track index
    }
};
```

## Implementation Phases

### Phase 1: Extract REAPER Code (1 hour)
- [ ] Move VST2 extensions (107 lines) → `REAPER/ReaperVST2Extensions.cpp`
- [ ] Move VST3 extensions (51 lines) → `REAPER/ReaperVST3Extensions.cpp`
- [ ] Move processReaperTimelineMidi (182 lines) → `REAPER/ReaperIntegration.cpp`
- [ ] This removes 340 lines from PluginProcessor.cpp!

### Phase 2: Create Pipeline Abstraction (2 hours)
- [ ] Create `Pipeline/MidiPipeline.h` interface
- [ ] Implement `Pipeline/StandardMidiPipeline.cpp` (existing logic)
- [ ] Implement `Pipeline/ReaperMidiPipeline.cpp` (fixed logic)
- [ ] Create `Pipeline/MidiPipelineFactory.cpp` for instantiation

### Phase 3: Add Caching Layer (1 hour)
- [ ] Implement `Midi/MidiCache.cpp` for timeline data
- [ ] Add fetch detection logic (only fetch new ranges)
- [ ] Implement incremental updates
- [ ] Add cleanup for old data

### Phase 4: Track Detection (30 min)
- [ ] Implement `REAPER/ReaperTrackDetector.cpp`
- [ ] Add auto-detection using TrackFX_GetFXName
- [ ] Add UI dropdown for manual track selection
- [ ] Store selected track in plugin state

### Phase 5: UI Refactor (1 hour)
- [ ] Extract callbacks to `UI/EditorCallbacks.cpp`
- [ ] Move timer logic to `UI/EditorTimer.cpp`
- [ ] Create `UI/EditorComponents.cpp` for menu setup
- [ ] Reduce PluginEditor.h from 365 to ~100 lines

### Phase 6: Testing (1 hour)
- [ ] Test standard VST pipeline (no regression)
- [ ] Test REAPER timeline mode
- [ ] Verify track detection works
- [ ] Performance testing (should be 60x faster!)

## Expected Results

### Performance Improvements
- **Before**: 280 lines executed 60 times/second = 16,800 operations/sec
- **After**: Fetch only when window moves = ~10 operations/sec
- **Improvement**: ~1,600x faster

### Code Quality
- **PluginProcessor.cpp**: 689 → 150 lines
- **PluginEditor.h**: 365 → 100 lines
- **Average file size**: ~150-200 lines
- **Separation**: REAPER code completely isolated

### Functionality
- ✅ Works with any track, not just Track 1
- ✅ Smooth playback without artifacts
- ✅ Proper lookahead for scrubbing
- ✅ Both pipelines work independently

## Critical Implementation Details

### Thread Safety Requirements

1. **noteStateMapArray Access**:
   - Audio thread WRITES (via pipeline)
   - GUI thread READS (via MidiInterpreter)
   - Must use juce::CriticalSection locks

2. **Cache Access**:
   - Audio thread WRITES (fetch/update)
   - Audio thread READS (process)
   - Single-threaded access, but needs lock for safety

3. **Display Window Communication**:
   - GUI sets display window size
   - Audio thread reads for fetch calculations
   - Use atomic variables or lock-free queue

### Pitfall Avoidance

1. **DON'T clear data on every fetch** - Add incrementally
2. **DON'T process modifiers as regular notes** - Two-pass system
3. **DON'T hardcode track index** - Auto-detect or UI select
4. **DON'T fetch in paint()** - Only in processBlock()
5. **DON'T duplicate existing cache entries** - Check before adding

### Integration Points

1. **ReaperMidiProvider Changes**:
   ```cpp
   // Add track index parameter
   getNotesInRange(double startPPQ, double endPPQ, int trackIndex = 0)
   ```

2. **MidiProcessor Access**:
   ```cpp
   // ReaperPipeline needs access to:
   - noteStateMapArray (write)
   - noteStateMapLock (lock)
   - gridlineMap (write)
   - gridlineMapLock (lock)
   ```

3. **Editor Communication**:
   ```cpp
   // Editor tells pipeline about display window
   pipeline->setDisplayWindow(startPPQ, endPPQ);
   ```

### Modifier Processing Order

**MUST process in this order:**
1. Tom markers (110-112) - Affects cymbal detection
2. HOPO/STRUM markers - Affects gem type
3. TAP markers (104) - Affects gem type
4. Star Power (116) - Visual only
5. Lanes (126-127) - Visual only
6. Playable notes - Uses above modifiers

## Migration Notes

1. **Backwards Compatibility**: Standard VST mode must not change
2. **State Preservation**: Plugin state format stays the same
3. **Testing**: Need to test in multiple DAWs
4. **REAPER Versions**: Test with REAPER 6.x and 7.x

## Success Criteria

1. No MIDI processing in paint()
2. Cache prevents redundant fetches
3. Track detection works automatically
4. Files under 200 lines each
5. Performance improved by >100x
6. No visual artifacts
7. Both pipelines work correctly

---

*This plan was created after comprehensive review of the codebase. Implementation should follow the phases sequentially to minimize risk.*