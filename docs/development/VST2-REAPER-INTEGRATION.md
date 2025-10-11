# VST2 REAPER Integration - Implementation Guide

**Status**: ✅ API Connection Working - Display Integration TODO
**Date**: October 9, 2025

## Current State

### What's Working ✅
- REAPER API connection via VST2 extensions
- `ReaperMidiProvider` reads MIDI from timeline with correct PPQ conversion (960:1 ratio)
- Green background indicator in REAPER
- All infrastructure ready for timeline-based display

### What's Next 🚧
- Implement display logic to show lookahead notes from timeline (not relying on latency buffer)
- Adjust rendering window to show notes approaching from ahead
- Wire up `processLookaheadMidi()` with proper visual window integration

## Overview

Integrates with REAPER's VST2 extensions for direct MIDI timeline reading, scrubbing support, and lookahead that bypasses VST audio buffer limitations.

## How It Works

REAPER exposes API via `audioMasterCallback`. Each function requested individually using magic numbers `0xdeadbeef` and `0xdeadf00d`:

```cpp
auto funcPtr = audioMaster(nullptr, 0xdeadbeef, 0xdeadf00d, 0, (void*)"FunctionName", 0.0);
auto GetPlayState = (int(*)())funcPtr;
int state = GetPlayState();
```

### Implementation Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                         REAPER Host                          │
└────────────────────────┬────────────────────────────────────┘
                         │ audioMasterCallback
                         ▼
┌─────────────────────────────────────────────────────────────┐
│              JUCE VST2 Wrapper (Modified)                    │
│  • Receives audioMaster callback                            │
│  • Calls handleVstHostCallbackAvailable()                   │
└────────────────────────┬────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────┐
│         ChartPreviewVST2Extensions                           │
│  • Stores callback in static member                         │
│  • Creates wrapper function pointer                         │
│  • Tests REAPER API availability                            │
└────────────────────────┬────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────┐
│         ChartPreviewAudioProcessor                           │
│  • Stores reaperGetFunc function pointer                    │
│  • Passes to ReaperMidiProvider                             │
└────────────────────────┬────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────┐
│            ReaperMidiProvider                                │
│  • Loads REAPER API functions                               │
│  • Reads MIDI from timeline                                 │
│  • Provides scrubbing support                               │
└─────────────────────────────────────────────────────────────┘
```

## Modified Files

### JUCE Framework Modifications

**1. `juce_audio_plugin_client_VST2.cpp`** - Expose callback in wrapper initialization
**2. `juce_VST2ClientExtensions.h`** - Add `handleVstHostCallbackAvailable()` virtual method

See `/docs/JUCE_REAPER_MODIFICATIONS.md` for details.

### Plugin Implementation

**3. `ChartPreview/Source/PluginProcessor.h`**
- Added `void* (*reaperGetFunc)(const char*)` - function pointer for API access
- Added `bool isReaperHost` - host detection flag
- Added `ReaperMidiProvider reaperMidiProvider` - MIDI timeline reader
- Added `std::unique_ptr<VST2ClientExtensions> vst2Extensions` - extensions instance

**4. `Source/PluginProcessor.cpp`** - `ChartPreviewVST2Extensions` class (lines 271-367):
- Advertises REAPER capabilities
- Receives audioMaster callback
- Tests and establishes API connection
- Uses static callback wrapper (lambda can't capture in function pointer)

**5. `Source/Midi/ReaperMidiProvider.h/cpp`** - REAPER API wrapper:
- `initialize()`, `getNotesInRange()`, `getCurrentPlayPosition()`, `getCurrentCursorPosition()`, `isPlaying()`
- Thread-safe with `juce::CriticalSection`

## Supported REAPER Capabilities

The plugin advertises support for:
- `reaper_vst_extensions` → Returns 1
- `hasCockosExtensions` → Returns `0xbeef0000`
- `hasCockosNoScrollUI` → Returns 1
- `hasCockosSampleAccurateAutomation` → Returns 1
- `wantsChannelCountNotifications` → Returns 1

## Build Configuration

### JUCE Submodule
- Location: `ChartPreview/third_party/JUCE`
- Version: 7.0.12
- Mode: Local copy with modifications
- VST2 SDK: Included in `juce_audio_plugin_client/pluginterfaces/`

### Projucer Configuration
`.jucer` file settings:
```xml
<JUCEOPTIONS JUCE_VST3_CAN_REPLACE_VST2="0"/>
<MODULEPATH id="juce_audio_plugin_client" path="third_party/JUCE/modules"/>
```

### Build Script
`./build-vst2.sh`:
1. Regenerates Xcode project with Projucer
2. Builds VST2 format
3. Installs to `~/Library/Audio/Plug-Ins/VST/ChartPreview.vst`

## Verification

1. Build: `./build-vst2.sh`
2. Load VST2 (not VST3) in REAPER
3. Check for green background and debug output: "✅ REAPER API connected via VST2"

## Usage in Code

### Accessing REAPER API
```cpp
// In ChartPreviewAudioProcessor
if (isReaperHost && reaperGetFunc)
{
    // Get any REAPER function by name
    auto GetNumTracks = (int(*)())reaperGetFunc("GetNumTracks");
    if (GetNumTracks)
    {
        int trackCount = GetNumTracks();
    }
}
```

### Reading Timeline MIDI
```cpp
// Get MIDI notes in a time range
auto notes = reaperMidiProvider.getNotesInRange(startPPQ, endPPQ);
for (const auto& note : notes)
{
    // note.pitch, note.velocity, note.startPPQ, note.endPPQ, etc.
}
```

### Checking Transport State
```cpp
bool playing = reaperMidiProvider.isPlaying();
double playPos = reaperMidiProvider.getCurrentPlayPosition();
double cursorPos = reaperMidiProvider.getCurrentCursorPosition();
```

## References

- [REAPER VST Extensions](https://www.reaper.fm/sdk/vst/vst_ext.php)
- [JUCE VST2ClientExtensions](https://docs.juce.com/master/structjuce_1_1VST2ClientExtensions.html)
- `Source/Midi/ReaperMidiProvider.cpp`

## Critical Discovery: PPQ Resolution

**IMPORTANT**: REAPER stores MIDI with internal resolution of **960 PPQ per quarter note**, while VST playhead reports in **quarter notes** (1 PPQ = 1 QN).

```cpp
// In ReaperMidiProvider::getNotesInRange()
const double REAPER_PPQ_RESOLUTION = 960.0;
double convertedStartPPQ = notStartPPQ / REAPER_PPQ_RESOLUTION;
double convertedEndPPQ = noteEndPPQ / REAPER_PPQ_RESOLUTION;
```

**Example:**
- Note at 51 seconds in REAPER timeline
- At 200 BPM: 51s × 3.33 QN/s = 170 QN
- REAPER reports: 23040 PPQ (internal)
- Converted: 23040 / 960 = 24 QN (matches VST playhead)

**Verification**:
- Guitar notes found at PPQ 3840, 14400, 23040 (REAPER internal)
- Converted to: 4, 15, 24 QN (matching playhead scale)
- At playhead position 12-19, the conversion works correctly

## Troubleshooting

**Green background not showing?**
- Make sure you loaded the VST2 version (not VST3)
- Check debug output for connection messages
- Verify VST2 SDK headers are present in JUCE submodule

**API functions returning nullptr?**
- Check that `isReaperHost` is true
- Verify function name spelling matches REAPER API exactly
- See `ReaperMidiProvider::loadReaperApiFunctions()` for examples

**No MIDI notes found?**
- Check PPQ conversion is applied (960:1 ratio)
- Verify items are MIDI, not audio (check `MIDI_CountEvts` return value)
- Ensure search range matches converted PPQ scale

**Build errors?**
- Ensure JUCE submodule is initialized: `git submodule update --init --recursive`
- Verify Projucer is in PATH or use full path in build script
- Check that VST2 SDK headers are in `third_party/JUCE/modules/juce_audio_plugin_client/pluginterfaces/`
