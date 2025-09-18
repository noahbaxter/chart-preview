# REAPER VST3 Integration - Technical Problem Statement

## What We're Actually Trying to Do

**Goal**: Read MIDI notes directly from REAPER's timeline (not from the VST buffer) to enable:
1. **Scrubbing**: When user drags the cursor while paused, immediately see MIDI notes at that position
2. **Lookahead**: Show notes ahead of the playhead without latency/buffer constraints

## The Specific REAPER API Functions We Need

From `third_party/reaper-sdk/sdk/reaper_plugin.h`, we need to call these functions:

```cpp
// Get MIDI note data from the timeline
MIDI_CountEvts(MediaItem_Take* take, int* notecnt, int* ccevtcnt, int* textsyxevtcnt)
MIDI_GetNote(MediaItem_Take* take, int noteidx, bool* selected, bool* muted,
             double* startppq, double* endppq, int* chan, int* pitch, int* vel)

// Get the current MIDI take context
GetActiveTake(MediaItem* item)
GetMediaItemTake_Item(MediaItem_Take* take)

// Get timeline position
GetPlayPosition2Ex(ReaProject* proj)
GetCursorPosition2Ex(ReaProject* proj)
```

## How REAPER Exposes These Functions to VST3 Plugins

REAPER provides a special VST3 interface `IReaperHostApplication` with this method:
```cpp
virtual void* getReaperApi(const char* funcname) = 0;
```

You call it like:
```cpp
auto MIDI_GetNote = (bool(*)(MediaItem_Take*, int, ...))reaperHost->getReaperApi("MIDI_GetNote");
```

## The Core Problem: Getting the IReaperHostApplication Interface

### How VST3 Interfaces Work (COM-style)
1. VST3 uses COM-style interfaces with `queryInterface()`
2. Each interface has a GUID (e.g., `{79655E36-77EE-4267-A573-FEF74912C27C}` for REAPER)
3. You query a host for an interface by its GUID

### What JUCE Provides
JUCE gives us `VST3ClientExtensions::setIHostApplication(Steinberg::FUnknown* host)` which is called by the host. But:
- `FUnknown` is only forward-declared in JUCE (no implementation)
- We can't directly call `queryInterface()` on it without the full VST3 SDK

### What We've Tried and Why It Failed

1. **Manual vtable call** → Crashed because vtable layout was wrong
2. **Using FUID class** → Linker error, JUCE doesn't provide implementation
3. **FUnknownPtr template** → Requires `IReaperHostApplication` to inherit from `FUnknown` properly

## The Reference Implementation That Works

The `JUCE-reaper-embedded-fx-gui` project works, but:
- They're using it for UI embedding, not MIDI reading
- They patched JUCE to add a new method that gets both `FUnknown*` AND `IHostApplication*`
- They use `FUnknownPtr<IReaperHostApplication>(hostContext)` which automatically queries

## Current Status

We have all the pieces ready:
- ✅ `ReaperMidiProvider` class that knows how to use REAPER API functions
- ✅ `MidiInterpreter::generateReaperTrackWindow()` that would use the data
- ✅ Visual indicators (red/green backgrounds) to show connection status
- ❌ Can't get `IReaperHostApplication*` from the host due to VST3/JUCE interface issues

## What Needs to Happen

We need ONE of these solutions:

### Option 1: Fix the Current Approach
Make `IReaperHostApplication` properly inherit from `FUnknown` and get the VST3 SDK linking correctly so `FUnknownPtr` works.

### Option 2: Use JUCE's Internal Mechanism
Find how JUCE internally handles `queryInterface` in their VST3 wrapper and use the same approach.

### Option 3: Patch JUCE (like reference project)
Add a method to AudioProcessor that receives the proper interface pointers from JUCE's VST3 wrapper.

### Option 4: Different Plugin Format
Consider if VST2 or another format would be easier (but lose VST3 benefits).

## The Actual Blocking Issue

The linker error `Undefined symbols: Steinberg::FUID::FUID()` happens because:
- JUCE includes VST3 SDK headers but not implementations
- The VST3 SDK classes expect to be linked against the full SDK
- JUCE uses a simplified wrapper that doesn't expose these internals

## Code That Should Work (If We Could Link Properly)

```cpp
void ChartPreviewAudioProcessor::ChartPreviewVST3Extensions::setIHostApplication(Steinberg::FUnknown* host)
{
    // This is the correct VST3 way, but won't link in JUCE:
    Steinberg::FUnknownPtr<IReaperHostApplication> reaperHost(host);

    if (reaperHost) {
        // We'd have the REAPER interface!
        auto MIDI_GetNote = reaperHost->getReaperApi("MIDI_GetNote");
        // ... could now read MIDI from timeline
    }
}
```

## Questions for Senior Dev

1. Is there a way to link against the full VST3 SDK while still using JUCE?
2. Can we access JUCE's internal VST3 wrapper to call queryInterface properly?
3. Should we just patch JUCE like the reference project did?
4. Is there a simpler approach we're missing?

## Files to Review

- `/Users/noahbaxter/Code/personal/chart-preview/ChartPreview/Source/PluginProcessor.cpp:247` - Where we try to get the interface
- `/Users/noahbaxter/Code/personal/chart-preview/ChartPreview/Source/Midi/ReaperMidiProvider.cpp` - What we'd do with the interface
- `/Users/noahbaxter/Code/personal/chart-preview/third_party/JUCE-reaper-embedded-fx-gui/PluginProcessor.cpp:39` - Working example (but uses patched JUCE)