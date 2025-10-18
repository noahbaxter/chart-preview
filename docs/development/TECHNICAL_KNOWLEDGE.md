# Technical Knowledge Base

A reference for difficult-to-discover implementation details and lessons learned during development.

---

## 🔧 **REAPER VST Integration** (Hard-Won Knowledge)

### The Problem
Stock JUCE cannot access REAPER's API. REAPER provides a C++ API for timeline data, MIDI notes, track info, etc., but accessing it from JUCE plugins requires source modifications.

### The Solution: JUCE Modifications

Every developer who wants full REAPER integration modifies JUCE:
- **Xenakios** (2016-2018): Maintained JUCE fork with REAPER modifications
- **GavinRay97** (2021): Created patches for VST3 REAPER UI embedding
- **This Project** (2025): Custom modifications for VST2 REAPER API access

### Our VST2 Implementation

**Files Modified**:
- `third_party/JUCE/modules/juce_audio_processors/utilities/juce_VST2ClientExtensions.h`
- `third_party/JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_VST2.cpp`

**Key Changes**:

1. **Added Method to VST2ClientExtensions** (juce_VST2ClientExtensions.h):
```cpp
virtual void handleReaperApi (void* (*reaperGetFunc)(const char*)) {}
```

2. **Modified VST2 Wrapper to Perform REAPER Handshake** (juce_audio_plugin_client_VST2.cpp):
```cpp
// Try to get REAPER API functions if we're in REAPER
auto reaperGetFunc = (void*(*)(const char*))audioMaster(nullptr, 0xdeadbeef, 0xdeadf00d, 0, nullptr, 0.0);
if (reaperGetFunc) {
    if (auto* callbackHandler = processorPtr->getVST2ClientExtensions()) {
        callbackHandler->handleReaperApi(reaperGetFunc);
    }
}
```

### The Handshake Magic Numbers
- `0xdeadbeef` - First magic number (opcode)
- `0xdeadf00d` - Second magic number (index)

These are standardized REAPER extension protocol numbers. They're not documented anywhere obvious - you have to find them by digging through REAPER SDK examples or reverse-engineering existing plugins.

### How to Use in Plugin Code

In your `AudioProcessor` subclass:

```cpp
class ChartPreviewAudioProcessor : public juce::AudioProcessor
{
public:
    VST2ClientExtensions* getVST2ClientExtensions() override
    {
        if (!vst2Extensions)
            vst2Extensions = std::make_unique<ChartPreviewVST2Extensions>();
        return vst2Extensions.get();
    }

private:
    class ChartPreviewVST2Extensions : public VST2ClientExtensions
    {
    public:
        void handleReaperApi(void* (*reaperGetFunc)(const char*)) override
        {
            // Store the function pointer
            processor->reaperGetFunc = reaperGetFunc;

            // Initialize REAPER integration
            processor->reaperMidiProvider.initialize(reaperGetFunc);
        }
    };

    std::unique_ptr<ChartPreviewVST2Extensions> vst2Extensions;
};
```

### Implementation in This Project

**Files**:
- `Source/PluginProcessor.h:38-54` - VST2 extensions declaration
- `Source/PluginProcessor.cpp:218-325` - VST2 extensions implementation
- `Source/Midi/ReaperMidiProvider.h/cpp` - REAPER API wrapper

**Integration Points**:
- `PluginProcessor.cpp:264` - `handleVstHostCallbackAvailable()` receives callback
- `PluginProcessor.cpp:273` - `tryGetReaperApi()` performs handshake
- `PluginProcessor.cpp:302` - Initializes `ReaperMidiProvider`

### Available REAPER Functions

Once you have the API, you can access hundreds of functions from `reaper_plugin.h`:

**Timeline Access**:
- `GetPlayPosition()`, `GetPlayPosition2Ex()`
- `GetCursorPosition()`, `GetCursorPosition2Ex()`
- `GetPlayState()` - Returns play/pause/record state

**MIDI Access**:
- `MIDI_CountEvts()` - Count MIDI events in a take
- `MIDI_GetNote()` - Get specific MIDI note data
- `MIDI_EnumSelNotes()` - Enumerate selected notes

**Project/Track Access**:
- `GetTrack()` - Get track by index
- `GetActiveTake()` - Get active MIDI take
- `GetMediaItem()` - Get media items
- `CountTracks()` - Get track count

See `third_party/reaper-sdk/sdk/reaper_plugin.h` for the full list (1000+ functions).

### VST3 Challenges (Not Yet Implemented)

VST3 support is harder because stock JUCE doesn't expose the host context properly. Two approaches:

1. **Use stock JUCE** `VST3ClientExtensions::setIHostApplication()` but requires manual `queryInterface()` handling and COM reference counting (messy)

2. **Modify JUCE** similar to VST2 approach (what GavinRay97 did in their patches)

Reference: [JUCE-reaper-embedded-fx-gui](https://github.com/GavinRay97/JUCE-reaper-embedded-fx-gui)

### Resources

**REAPER SDK**:
- `third_party/reaper-sdk/sdk/reaper_plugin.h` - Full API function declarations
- `third_party/reaper-sdk/sdk/reaper_vst3_interfaces.h` - VST3 interface definitions

**JUCE Examples**:
- `/Applications/JUCE/examples/Plugins/ReaperEmbeddedViewPluginDemo.h` - Official JUCE example (UI embedding, not MIDI API)

**External Projects**:
- [JUCE-reaper-embedded-fx-gui](https://github.com/GavinRay97/JUCE-reaper-embedded-fx-gui) - GavinRay97's VST3 patches
- [Xenakios's Bitbucket](https://bitbucket.org/xenakios/reaper_api_vst_example) - Historical VST2 example

**Forum Discussions**:
- [JUCE Forum: Attaching to REAPER API VST2 vs VST3](https://forum.juce.com/t/attaching-to-the-reaper-api-vst2-vs-vst3/45459)
- [Cockos Forum: JUCE plugin using REAPER VST extensions](https://forums.cockos.com/showthread.php?t=253505)
- [JUCE GitHub Issue #902](https://github.com/juce-framework/JUCE/issues/902) - Custom VST3 interface registration

---

## 🎨 **3D Perspective Rendering Math**

### Location
`ChartPreview/Source/Visual/Renderers/HighwayRenderer.cpp` - `createPerspectiveGlyphRect()` and related functions

### The Problem
Converting 2D highway positions to 3D perspective-aware positions with proper scaling for depth illusion is complex and undocumented in the original code.

### Key Constants
```cpp
const float highwayDepth = 100.0f;           // Distance to highway vanishing point
const float playerDistance = 50.0f;          // Player eye position relative to highway
const float perspectiveStrength = 0.7f;      // How much to exaggerate perspective (0-1)
const float exponentialCurve = 0.5f;         // Exponential curve factor for smooth scaling
const float xOffsetMultiplier = 0.5f;        // X offset strength at depth
const float barNoteHeightRatio = 16.0f;      // Height scaling for bar/kick notes
const float regularNoteHeightRatio = 2.0f;   // Height scaling for regular notes
```

### The Math
Notes at different depths are scaled based on their distance from the player:
- Notes far away (low on screen) appear smaller
- Notes close (high on screen) appear larger
- X-axis scaling creates left/right perspective lines
- Exponential curve provides smooth non-linear scaling

### Critical Implementation Detail
The perspective calculation happens in multiple places:
- `getGuitarGlyphRect()` / `getDrumGlyphRect()` - Gem positioning
- `getGuitarGridlineRect()` / `getDrumGridlineRect()` - Gridline positioning
- `getGuitarLaneCoordinates()` / `getDrumLaneCoordinates()` - Lane/sustain positioning

**These 6 functions use almost identical perspective math** - any change to the perspective algorithm must be applied to all 6 places. This is a major source of bugs if they drift apart.

### Future Improvement
Extract to parameterized function:
```cpp
juce::Rectangle<float> applyPerspective(const LayoutParams& params, float depth);
```

---

## 🔐 **Thread Safety & Race Conditions**

### Critical Issue: Black Screen Bug (v0.8.6)

**The Problem**:
Audio thread and GUI thread both access `noteStateMapArray`. A race condition could cause:
1. Audio thread clears the array
2. GUI thread starts reading (expects data)
3. Audio thread writes new data
4. Result: GUI renders empty array for a frame = black screen

**The Fix**:
In `ReaperMidiPipeline::processCachedNotesIntoState()`, hold `noteStateMapLock` for the **entire** clear+write operation:

```cpp
{
    juce::ScopedLock lock(noteStateMapLock);
    noteStateMapArray.clear();      // Atomic with...
    // ... write all notes into array ...
}  // lock released
```

**Key Learning**: Always use atomic clear+write when updating shared data structures read by rendering thread. Never split the operation.

### Current Thread Safety Implementation

**Protected Data Structures**:
- `NoteStateMapArray` - Protected by `noteStateMapLock`
- `GridlineMap` - Protected by `gridlineMapLock`
- `HitAnimationManager` - Per-column state, guarded by locks

**Audio Thread Responsibilities**:
- Process MIDI events
- Update `noteStateMapArray`
- Update `GridlineMap`
- Cache invalidation in REAPER mode

**GUI Thread Responsibilities**:
- Read from `noteStateMapArray` (takes lock)
- Read from `GridlineMap` (takes lock)
- Render to screen
- Handle user input

---

## 📊 **PPQ Timing System**

### Why PPQ (Pulses Per Quarter)?

Music timing in DAWs uses PPQ because it's tempo-independent:
- 1 quarter note = 1 PPQ unit
- Works the same at 60 BPM or 300 BPM
- Scales naturally when tempo changes

### Implementation Details

**PPQ Conversions**:
- `timeToProgress()` - Samples → PPQ
- `progressToTime()` - PPQ → Samples
- Uses `currentBPM` and `currentTimeSignature` from `GridlineMap`

**Critical Point**: All note timing stored and compared as PPQ, never as samples. This eliminates tempo-related bugs.

### Absolute vs Relative Positioning

**Old System (Relative)**: Positions relative to playback start
- ❌ Problem: Scrubbing breaks when you jump to middle of song
- ❌ Problem: Tempo changes cause stretching

**New System (Absolute)**: Positions from project start (PPQ 0)
- ✅ Always consistent
- ✅ Scrubbing works perfectly
- ✅ Tempo changes don't cause visual glitches

---

## 🚀 **Performance Considerations**

### Draw Call Batching

Rendering efficiency achieved by:
1. Collecting all draw operations into `DrawCallMap` by type
2. Sorting by depth/layer
3. Executing in order (back to front)

### MIDI Caching Strategy

REAPER pipeline uses smart caching:
- Cache entire track's MIDI at project start
- Invalidate on:
  - Track switching
  - Project changes
  - MIDI edits detected (hash-based)
- Subsequent frames hit cache (very fast)

### Debug Logging Optimization

⚠️ **Critical**: String concatenation for debug output happens only when debug console is open, preventing performance degradation during normal playback.

---

## 🐛 **Known Difficult Issues**

### Motion Sickness from Scrolling (Unsolved)

Users report motion sickness with scroll speed. Possible causes:
- Scroll acceleration/deceleration not smooth
- Frame drops causing visual stuttering
- Scroll speed too fast or unpredictable
- Animation smoothness issues

**Investigation Needed**:
- Profile frame times during scrolling
- Test with lower FPS caps (15/30)
- Examine scroll smoothing algorithms
- Consider frame-time based scrolling vs tick-based

### Star Power Rendering (Incomplete)

Three separate TODOs indicate Star Power rendering architecture is uncertain:
```cpp
// TODO: Need to convert frameTime back to PPQ for star power check
// TODO: Need to pass SP state differently
// TODO: Need to pass SP state differently for time-based rendering
```

**Current Status**: Feature partially working but architectural approach unclear. Either complete or remove.

---

## 📝 **Notes for Future Developers**

1. **Always check `third_party/JUCE/` first**, not `/Applications/JUCE/` (custom modifications)

2. **PPQ timing is the foundation** - understand this before modifying rendering

3. **Thread safety is critical** - every shared data structure needs lock protection

4. **Test on all three platforms** - Windows, macOS, Linux have different VST behaviors

5. **REAPER integration is fragile** - magic numbers and API handshakes are easy to break

6. **Performance matters** - 60 FPS default with complex math means every millisecond counts

---

**Last Updated**: 2025-10-18
