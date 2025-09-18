# REAPER Integration Documentation

## Goal
Enable ChartPreview VST3 to read MIDI notes directly from REAPER's timeline for:
1. **Scrubbing** - Instant visual feedback when dragging cursor while paused
2. **Lookahead** - Show notes ahead without buffer/latency constraints

## Current Status
- ✅ Phase 1 Complete: Cursor vs playhead position tracking works
- ❌ Phase 2 Blocked: Cannot get REAPER VST3 interface due to JUCE/VST3 linking issues

## Technical Blocker
Cannot call `queryInterface()` on `FUnknown*` to get `IReaperHostApplication*` because:
- JUCE only provides VST3 headers, not implementations
- Full VST3 SDK classes won't link with JUCE's simplified wrapper
- See [REAPER_INTEGRATION_TECHNICAL_PROBLEM.md](./REAPER_INTEGRATION_TECHNICAL_PROBLEM.md) for detailed analysis

## Resources

### REAPER SDK Files
- `third_party/reaper-sdk/sdk/reaper_plugin.h` - REAPER API function declarations
- `third_party/reaper-sdk/sdk/reaper_vst3_interfaces.h` - VST3 interface definition

### Reference Implementation
- `third_party/JUCE-reaper-embedded-fx-gui/` - Working example (but for UI embedding, uses patched JUCE)

### Key Forum Discussions
- [JUCE Forum: Attaching to REAPER API VST2 vs VST3](https://forum.juce.com/t/attaching-to-the-reaper-api-vst2-vs-vst3/45459)
- [Cockos Forum: REAPER VST UI embedding merged to JUCE](https://forum.cockos.com/showthread.php?t=254565)

## Implementation Files

### Core Implementation
- `Source/Midi/ReaperMidiProvider.h/cpp` - Interface to REAPER API (ready but can't connect)
- `Source/Midi/MidiInterpreter.cpp` - `generateReaperTrackWindow()` at line 260 (ready but returns empty)
- `Source/PluginProcessor.cpp` - `setIHostApplication()` at line 247 (where connection fails)

### Visual Testing
- `Source/PluginEditor.cpp` - Lines 132-165 show colored backgrounds:
  - Green = REAPER connected
  - Red = REAPER detected but failed
  - Blue = No REAPER provider
  - Yellow = Buffer mode (non-REAPER build)

## Build Configuration
Add to `.jucer` file preprocessor definitions:
```
REAPER_EXTENSION=1
```

Currently using `#ifdef REAPER_EXTENSION` for conditional compilation.