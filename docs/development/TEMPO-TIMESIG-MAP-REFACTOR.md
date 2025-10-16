# TempoTimeSignatureMap Refactor Plan

## Problem Statement

The current REAPER pipeline has a critical performance issue that causes CPU spikes and eventually makes the host unresponsive over time:

1. **`buildGridlines()`** is called frequently and generates gridlines for large PPQ ranges (up to 16 beats)
2. **`gridlineMap`** accumulates thousands of gridline entries over time (every half-beat gets an entry)
3. Even with cleanup, the map grows faster than it's cleaned up
4. **Memory bloat** → slower map operations → CPU spike → unresponsive UI

This happens especially when calling `invalidateReaperCache()` every frame (or subset of frames) to enable real-time MIDI editing while paused.

---

## Solution Architecture

Replace pre-generated `gridlineMap` with just-in-time gridline generation from a minimal `TempoTimeSignatureMap`.

### Key Changes:
- **Store only tempo/timesig change points** in `TempoTimeSignatureMap` (typically 1-5 entries per section)
- **Query REAPER API** for tempo/timesig changes in the current render window
- **Generate gridlines on-the-fly** during rendering in `TimeConverter`
- **Aggressive cleanup** to prevent data accumulation

### Benefits:
- **Minimal Memory**: Only change points stored (1-5 entries vs thousands)
- **No Accumulation**: Aggressive cleanup prevents bloat
- **Accurate**: REAPER's tempo map API gives exact tempo/timesig info
- **Clean Architecture**: Separation of storage vs rendering

---

## Implementation Phases

### Phase 1: Add REAPER Tempo/TimeSig Query API

**Files: `ReaperMidiProvider.h/cpp`**

Add new method to query REAPER's tempo map:

```cpp
// In ReaperMidiProvider.h
std::vector<TempoTimeSignatureEvent> getTempoTimeSignatureEventsInRange(double startPPQ, double endPPQ);
```

**Implementation details:**
- Query REAPER's `TimeMap2_*` API functions (already loaded)
- Return all tempo/timesig events in the specified PPQ range
- **IMPORTANT**: Include at least 1 event before `startPPQ` if available (for current tempo/timesig context)
- Use REAPER's bar/beat calculation functions to get time signatures
- Use REAPER's tempo functions to get BPM at each position

---

### Phase 2: Modify ReaperMidiPipeline

**File: `ReaperMidiPipeline.h/cpp`**

#### 2a. Add new method to fetch tempo/timesig events

```cpp
void ReaperMidiPipeline::fetchTempoTimeSignatureEvents(PPQ start, PPQ end)
{
    // Query REAPER for tempo/timesig changes in this range
    auto events = reaperProvider.getTempoTimeSignatureEventsInRange(
        start.toDouble(),
        end.toDouble()
    );

    // Store in MidiProcessor's tempoTimeSignatureMap
    const juce::ScopedLock lock(midiProcessor.tempoTimeSignatureMapLock);
    for (const auto& event : events) {
        midiProcessor.tempoTimeSignatureMap[event.ppqPosition] = event;
    }
}
```

#### 2b. Update `processCachedNotesIntoState()`

**Remove:**
```cpp
buildGridlines(currentPos, currentPos + PPQ(MAX_HIGHWAY_LENGTH),
               timeSignatureNumerator, timeSignatureDenominator);
```

**Replace with:**
```cpp
// Fetch tempo/timesig events for the display window only
PPQ fetchStart = currentPos - PPQ(PREFETCH_BEHIND);
PPQ fetchEnd = currentPos + displayWindowSize + PPQ(PREFETCH_AHEAD);
fetchTempoTimeSignatureEvents(fetchStart, fetchEnd);
```

#### 2c. Add cleanup for TempoTimeSignatureMap

Add at the end of `processCachedNotesIntoState()`:

```cpp
// Cleanup tempo/timesig events outside the window (keep only what's needed)
{
    const juce::ScopedLock lock(midiProcessor.tempoTimeSignatureMapLock);
    auto& map = midiProcessor.tempoTimeSignatureMap;

    PPQ windowStart = currentPos - PPQ(PREFETCH_BEHIND);
    PPQ windowEnd = currentPos + displayWindowSize + PPQ(PREFETCH_AHEAD);

    // Keep the event right before windowStart (so we know current tempo/timesig)
    auto lower = map.lower_bound(windowStart);
    if (lower != map.begin()) --lower;
    map.erase(map.begin(), lower);

    // Remove everything after windowEnd
    auto upper = map.upper_bound(windowEnd);
    map.erase(upper, map.end());
}
```

---

### Phase 3: Remove GridlineMap from MidiProcessor

**File: `MidiProcessor.h/cpp`**

#### Remove the following:

1. **Member variables:**
   - `GridlineMap gridlineMap`
   - `juce::CriticalSection gridlineMapLock`

2. **Methods:**
   - `buildGridlineMap()` method (entire implementation)

3. **Cleanup code in `cleanupOldEvents()`** (lines 142-154):
   ```cpp
   // Remove this entire block:
   // Erase gridlines in PPQ range
   {
       const juce::ScopedLock lock(gridlineMapLock);
       auto lower = gridlineMap.upper_bound(conservativeStartPPQ);
       if (lower != gridlineMap.begin())
       {
           --lower;
           gridlineMap.erase(gridlineMap.begin(), lower);
       }

       auto upper = gridlineMap.upper_bound(conservativeEndPPQ);
       gridlineMap.erase(upper, gridlineMap.end());
   }
   ```

4. **Cleanup code in `clearNoteDataInRange()`** (lines 276-282):
   ```cpp
   // Remove this entire block:
   // Also clear gridlines in this range
   {
       const juce::ScopedLock gridLock(gridlineMapLock);
       auto lower = gridlineMap.lower_bound(startPPQ);
       auto upper = gridlineMap.upper_bound(endPPQ);
       gridlineMap.erase(lower, upper);
   }
   ```

5. **Call to `buildGridlineMap()` in `process()`** (line 25):
   ```cpp
   // Remove this line:
   buildGridlineMap(startPPQ, endPPQ, timeSig->numerator, timeSig->denominator);
   ```

#### Keep the following:

- `TempoTimeSignatureMap tempoTimeSignatureMap`
- `juce::CriticalSection tempoTimeSignatureMapLock`
- Cleanup for `tempoTimeSignatureMap` in `cleanupOldEvents()` (lines 156-168) - this is correct
- Cleanup for `tempoTimeSignatureMap` in `clearNoteDataInRange()` (lines 284-290) - this is correct

---

### Phase 4: Update MidiInterpreter

**File: `MidiInterpreter.h/cpp`**

#### Remove:

**From `MidiInterpreter.h`:**
```cpp
GridlineMap generateGridlineWindow(PPQ trackWindowStart, PPQ trackWindowEnd);
```

**From `MidiInterpreter.cpp`:**
```cpp
// Remove entire method implementation (lines 26-47):
GridlineMap MidiInterpreter::generateGridlineWindow(PPQ trackWindowStart, PPQ trackWindowEnd)
{
    GridlineMap gridlineWindow;

    // Lock the gridlineMap during iteration to prevent crashes
    const juce::ScopedLock lock(gridlineMapLock);

    // Simply read from the pre-generated gridlineMap
    for (const auto& gridlineItem : gridlineMap)
    {
        PPQ gridlinePPQ = gridlineItem.first;
        Gridline gridlineType = static_cast<Gridline>(gridlineItem.second);

        // Only include gridlines within our window
        if (gridlinePPQ >= trackWindowStart && gridlinePPQ < trackWindowEnd)
        {
            gridlineWindow[gridlinePPQ] = gridlineType;
        }
    }

    return gridlineWindow;
}
```

#### Keep:

- `generateGridlinesFromTempoTimeSignatureMap()` - This becomes the canonical way to generate gridlines (though it won't be called directly in the new architecture)

---

### Phase 5: Update Rendering (PluginEditor)

**File: `PluginEditor.cpp`**

#### For REAPER Mode (around line 248):

**Replace:**
```cpp
GridlineMap ppqGridlineMap = midiInterpreter.generateGridlineWindow(extendedStart, trackWindowEndPPQ);
```

**With:**
```cpp
// Get tempo/timesig map from MidiProcessor (make a copy to avoid holding lock)
TempoTimeSignatureMap tempoTimeSigMap;
{
    const juce::ScopedLock lock(audioProcessor.getMidiProcessor().tempoTimeSignatureMapLock);
    tempoTimeSigMap = audioProcessor.getMidiProcessor().tempoTimeSignatureMap;
}

// Generate gridlines on-the-fly directly to time-based format
TimeBasedGridlineMap timeGridlines = TimeConverter::convertTempoTimeSignatureMap(
    tempoTimeSigMap,
    extendedStart,
    trackWindowEndPPQ,
    cursorPPQ,
    ppqToTime
);
```

**Then update the conversion section (around line 254):**

Remove the old `TimeConverter::convertGridlineMap()` call:
```cpp
// Remove this line:
TimeBasedGridlineMap timeGridlines = TimeConverter::convertGridlineMap(ppqGridlineMap, cursorPPQ, ppqToTime);
```

(It's already replaced by the code above)

#### For Standard Mode (around line 320):

**Leave unchanged for now** - Standard MIDI mode continues using `generateGridlineWindow()` from the pre-built `gridlineMap`.

**Note**: This will break when we remove `gridlineMap` in Phase 3. We'll need to either:
- Keep building `gridlineMap` only for standard mode
- OR update standard mode to use a similar tempo/timesig approach (with default 120 BPM, 4/4)

**Recommendation**: For now, add a fallback in standard mode that builds a simple default tempo/timesig map:
```cpp
// Standard mode: Create a simple default tempo/timesig map if it doesn't exist
TempoTimeSignatureMap tempoTimeSigMap;
{
    const juce::ScopedLock lock(audioProcessor.getMidiProcessor().tempoTimeSignatureMapLock);
    tempoTimeSigMap = audioProcessor.getMidiProcessor().tempoTimeSignatureMap;

    // If empty, add a default 4/4, 120 BPM event at the start
    if (tempoTimeSigMap.empty()) {
        tempoTimeSigMap[PPQ(0.0)] = TempoTimeSignatureEvent(PPQ(0.0), currentBPM, 4, 4);
    }
}

// Generate gridlines on-the-fly directly to time-based format
TimeBasedGridlineMap timeGridlines = TimeConverter::convertTempoTimeSignatureMap(
    tempoTimeSigMap,
    extendedStart,
    trackWindowEndPPQ,
    cursorPPQ,
    ppqToTime
);
```

---

### Phase 6: Verify TimeConverter Implementation

**File: `TimeConverter.h`**

Confirm `convertTempoTimeSignatureMap()` exists and is correct (lines 124-218). This method:

1. Takes parameters:
   - `TempoTimeSignatureMap` - the tempo/timesig events
   - `startPPQ`, `endPPQ` - the range to generate gridlines for
   - `cursorPPQ` - the current playback position
   - `ppqToTime` - lambda function to convert PPQ to absolute time

2. Generates gridlines by:
   - Iterating through tempo/timesig sections
   - Calculating measure length from numerator/denominator: `measureLength = numerator * (4.0 / denominator)`
   - Generating MEASURE gridlines at measure boundaries
   - Generating BEAT gridlines at beat boundaries (every 1 PPQ)
   - Generating HALF_BEAT gridlines at half-beat boundaries (every 0.5 PPQ)

3. Returns `TimeBasedGridlineMap` (vector of `TimeBasedGridline` structs) ready for rendering

**No changes needed** - this implementation is already correct.

---

## Testing & Validation

### Manual Testing:

1. **Test REAPER mode with simple tempo:**
   - Load plugin in REAPER with constant 120 BPM, 4/4 time
   - Verify gridlines appear correctly
   - Verify no performance degradation over time

2. **Test REAPER mode with tempo changes:**
   - Add tempo changes in REAPER project
   - Verify gridlines adjust correctly at tempo change boundaries

3. **Test REAPER mode with time signature changes:**
   - Change time signature (e.g., 4/4 to 3/4)
   - Verify measure lines appear at correct intervals

4. **Test standard MIDI mode:**
   - Load plugin in non-REAPER DAW
   - Verify gridlines still work correctly

5. **Test performance:**
   - Leave plugin paused for extended period
   - Monitor CPU usage - should remain stable
   - Previously: CPU would spike to 100% over time

### Expected Behavior:

- **Memory usage**: `tempoTimeSignatureMap` should contain 1-10 entries max at any time
- **CPU usage**: Should remain stable even with frequent cache invalidation
- **Visual output**: Gridlines should look identical to previous implementation
- **Tempo accuracy**: Gridlines should match REAPER's tempo map exactly

---

## Implementation Notes

### Standard MIDI Mode Considerations:

The current plan leaves standard MIDI mode using the old `gridlineMap` approach temporarily. Options for future work:

1. **Option A**: Keep `buildGridlineMap()` only for standard mode
   - Pros: Minimal changes to standard mode
   - Cons: Code duplication, still have gridlineMap

2. **Option B**: Update standard mode to use tempo/timesig map with default values
   - Pros: Unified architecture, cleaner code
   - Cons: More changes required

**Recommendation**: Go with Option B - it's cleaner long-term and the changes are straightforward.

### REAPER API Functions Needed:

The following REAPER API functions are already loaded and available:
- `TimeMap2_QNToTime` - Convert quarter notes to time
- `TimeMap2_timeToQN` - Convert time to quarter notes
- `TimeMap2_timeToBeats` - Get bar/beat info at time position
- `TimeMap_GetDividedBpmAtTime` - Get tempo at time position

These provide all the data needed for `getTempoTimeSignatureEventsInRange()`.

---

## Rollback Plan

If issues arise during implementation:

1. **Revert Phase 5 first** - restore old gridline generation in PluginEditor
2. **Revert Phase 3** - restore `gridlineMap` and `buildGridlineMap()`
3. **Keep Phase 1 & 2** - the tempo/timesig query infrastructure is useful for future work

---

## Success Criteria

✅ Plugin loads and displays gridlines correctly in REAPER mode
✅ Plugin loads and displays gridlines correctly in standard mode
✅ CPU usage remains stable over extended periods (30+ minutes paused)
✅ Memory usage of tempo/timesig map stays under 1KB (vs MB for gridlineMap)
✅ Real-time MIDI editing works smoothly (with frequent cache invalidation)
✅ Tempo changes in REAPER are reflected accurately in gridlines
✅ Time signature changes in REAPER are reflected accurately in gridlines

---

## Files Modified Summary

1. **ReaperMidiProvider.h/cpp** - Add tempo/timesig query method
2. **ReaperMidiPipeline.h/cpp** - Replace buildGridlines with fetchTempoTimeSignatureEvents
3. **MidiProcessor.h/cpp** - Remove gridlineMap, keep tempoTimeSignatureMap
4. **MidiInterpreter.h/cpp** - Remove generateGridlineWindow
5. **PluginEditor.cpp** - Update rendering to use TimeConverter::convertTempoTimeSignatureMap
6. **TimeConverter.h** - No changes needed (already has correct implementation)
