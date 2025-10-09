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

This plugin successfully integrates with REAPER's VST2 extensions to access the REAPER API, enabling direct MIDI timeline reading, scrubbing support, and lookahead functionality that bypasses VST audio buffer limitations.

## How It Works

### REAPER API Access Method

REAPER exposes its API to VST2 plugins through the `audioMasterCallback`. Each API function is requested individually:

```cpp
// Request a REAPER function by name
auto funcPtr = audioMaster(nullptr, 0xdeadbeef, 0xdeadf00d, 0, (void*)"FunctionName", 0.0);

// Cast and call the function
auto GetPlayState = (int(*)())funcPtr;
int state = GetPlayState();
```

**Key Insight**: There is no single "getReaperApi" bootstrapping function. Each function must be requested by name using magic numbers `0xdeadbeef` and `0xdeadf00d`.

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

**1. `third_party/JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_VST2.cpp`**

Added callback exposure in the VST2 wrapper initialization:
```cpp
// After VST2 wrapper setup, expose callback to plugin
if (auto* vst2Extensions = processor->getVST2ClientExtensions())
{
    vst2Extensions->handleVstHostCallbackAvailable([this](int32 opcode, ...) {
        return audioMaster(effect, opcode, ...);
    });
}
```

**2. `third_party/JUCE/modules/juce_audio_processors/utilities/juce_VST2ClientExtensions.h`**

Added virtual method for callback reception:
```cpp
virtual void handleVstHostCallbackAvailable(std::function<VstHostCallbackType>&& callback);
```

### Plugin Implementation

**3. `ChartPreview/Source/PluginProcessor.h`**
- Added `void* (*reaperGetFunc)(const char*)` - function pointer for API access
- Added `bool isReaperHost` - host detection flag
- Added `ReaperMidiProvider reaperMidiProvider` - MIDI timeline reader
- Added `std::unique_ptr<VST2ClientExtensions> vst2Extensions` - extensions instance

**4. `ChartPreview/Source/PluginProcessor.cpp`**

Implemented `ChartPreviewVST2Extensions` class (lines 271-367):
- `handleVstPluginCanDo()` - Advertises REAPER capabilities
- `handleVstManufacturerSpecific()` - Handles custom plugin naming
- `handleVstHostCallbackAvailable()` - Receives audioMaster callback
- `tryGetReaperApi()` - Tests and establishes REAPER API connection

Key implementation detail - static callback wrapper:
```cpp
// Store callback in static member (lambda can't capture in function pointer)
static std::function<VstHostCallbackType>* staticCallback;

// Create captureless lambda wrapper
static auto reaperApiWrapper = [](const char* funcname) -> void* {
    auto& callback = *ChartPreviewVST2Extensions::staticCallback;
    return (void*)callback(0xdeadbeef, 0xdeadf00d, 0, (void*)funcname, 0.0);
};

processor->reaperGetFunc = reaperApiWrapper;
```

**5. `ChartPreview/Source/Midi/ReaperMidiProvider.h/cpp`**

Comprehensive REAPER API wrapper providing:
- `initialize(reaperGetFunc)` - Loads all required REAPER functions
- `getNotesInRange(startPPQ, endPPQ)` - Reads MIDI notes from timeline
- `getCurrentPlayPosition()` - Gets playback position
- `getCurrentCursorPosition()` - Gets edit cursor position
- `isPlaying()` - Checks transport state

Thread-safe with `juce::CriticalSection` and comprehensive error handling.

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

To verify REAPER integration is working:

1. **Build the plugin**:
   ```bash
   cd ChartPreview
   ./build-vst2.sh
   ```

2. **Load in REAPER**:
   - Open REAPER
   - Add VST2 plugin (not VST3!)
   - Select "Chart Preview"

3. **Check for indicators**:
   - Green background tint (visual confirmation)
   - Debug output: "✅ REAPER API connected via VST2"

4. **Test API access**:
   - Debug output shows GetPlayState() result
   - ReaperMidiProvider initialized successfully

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

- [REAPER VST Extensions Documentation](https://www.reaper.fm/sdk/vst/vst_ext.php)
- [JUCE VST2ClientExtensions API](https://docs.juce.com/master/structjuce_1_1VST2ClientExtensions.html)
- ReaperMidiProvider: `Source/Midi/ReaperMidiProvider.cpp`
- Previous research: `docs/archive/REAPER-VST2-INTEGRATION-REPORT-2025-10-09-research.md`

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
