# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Chart Preview is a VST/AU plugin for DAWs that visualizes MIDI notes as rhythm game charts (similar to Clone Hero/YARG). It's built with JUCE framework and designed for macOS and Windows 64-bit DAWs.

## Development Commands

### Building the Plugin

**macOS (Primary Development)**
```bash
cd "Chart Preview"
./build.sh
```

The build script handles:
- Regenerating Xcode project from Projucer if needed
- Building VST3 format by default
- Installing to `~/Library/Audio/Plug-Ins/VST3/`
- Opening REAPER test project automatically
- Closing REAPER before build to avoid file locks

**Build Types:**
- `vst` (default): VST3 plugin
- `standalone`: Standalone application  
- `au`: Audio Unit plugin

**Windows**
Use Visual Studio 2022 project at `Chart Preview/Builds/VisualStudio2022/ChartPreview.sln`

### Projucer Integration

The main project file is `Chart Preview/ChartPreview.jucer`. When modified:
- The build script automatically regenerates Xcode project
- Manual regeneration: `/Applications/JUCE/Projucer.app/Contents/MacOS/Projucer --resave "ChartPreview.jucer"`

### Testing Setup

- REAPER project: `reaper-test/reaper-test.rpp`
- Ableton project: `ableton-test Project/ableton-test.als`
- Test MIDI: `reaper-test/notes.mid`

## Architecture

### Core Components

**Audio Processing Chain:**
1. `ChartPreviewAudioProcessor` - Main plugin processor
2. `MidiProcessor` - Processes MIDI events and maintains note state
3. `MidiInterpreter` - Converts MIDI data to visual chart elements
4. `HighwayRenderer` - Renders the chart visualization
5. `ChartPreviewAudioProcessorEditor` - UI and real-time display

**Key Data Structures:**
- `NoteStateMapArray` - Tracks held notes across all MIDI pitches
- `TrackWindow` - Frame-based chart data for rendering window
- `SustainWindow` - Sustain note data for rendering window
- `GridlineMap` - Beat/measure grid timing information
- `SustainList` - Sustain note information (for guitar)

### Threading Model

- **Audio Thread**: Processes MIDI, updates note states and gridlines
- **GUI Thread**: Renders visualization, handles UI interactions
- **Thread Safety**: Uses `juce::CriticalSection` for `gridlineMap` access

### Timing System

All timing uses **PPQ (Pulses Per Quarter)** for tempo-independent calculations:
- MIDI events stored with PPQ timestamps
- Rendering window calculated in PPQ space
- Latency compensation applied in PPQ

### Instrument Support

**Drums (Feature Complete)**
- Normal and Pro drums modes
- Ghost notes and accents (dynamics)
- 2x kick support
- Cymbal vs tom differentiation
- Lanes (in development)

**Guitar (Partially Implemented)**
- Open notes and tap notes supported
- HOPOs (hammer-ons/pull-offs) 
- Sustain notes (partially implemented)
- Lanes (in development)

## Important Implementation Details

### MIDI Pitch Mappings

Defined in `Utils.h` as `MidiPitchDefinitions`:
- Separate mappings for Guitar and Drums
- Multiple difficulty levels (Easy, Medium, Hard, Expert)
- Special pitches for star power, dynamics, lanes

### Asset Management

- Graphics assets in `Chart Preview/Assets/`
- Audio assets in `Chart Preview/Audio/`
- `AssetManager` handles loading and caching
- Perspective rendering for 3D highway effect
- Draw call optimization using `DrawCallMap`

### Windows Compatibility

Includes Windows-specific typedef in `Utils.h`:
```cpp
#if defined(_WIN32) || defined(_WIN64) || defined(__WINDOWS__) || defined(_MSC_VER)
    typedef unsigned int uint;
#endif
```

### Performance Considerations

- 60 FPS rendering by default (configurable: 15/30/60 FPS)
- Latency compensation with multi-buffer smoothing
- Chart zoom affects visible note count (400ms to 1.5s)
- Draw call batching and culling optimizations in progress

## State Management

Plugin state stored in `juce::ValueTree` with properties:
- `part`: Guitar/Drums selection
- `skillLevel`: Easy/Medium/Hard/Expert
- `framerate`: 15/30/60 FPS
- `latency`: 250ms/500ms/750ms/1000ms/1500ms
- `starPower`, `kick2x`, `dynamics`: Visibility toggles

## Development Status

See `TODO.txt` for detailed priority-ranked backlog. Recent completions:
- ✅ PPQ-based timing system conversion
- ✅ Latency compensation with multi-buffer smoothing
- ✅ Grid visual polish (beat/half-beat/measure markers)
- ✅ CI/CD pipeline automation

Key remaining areas:
- **P0**: Thread safety improvements, parameter system migration
- **P1**: Complete guitar sustains, extended memory, drums lanes
- **P2**: Performance optimizations, DPI scaling
- **P3**: Advanced features, Real Drums support

## File Structure Notes

- Main source in `Chart Preview/Source/`
- JUCE auto-generated files in `JuceLibraryCode/`
- Third-party dependencies in `ThirdParty/`
- Platform-specific builds in `Builds/`
- Asset files separate from distributable art assets
- `DO NOT DISTRIBUTE/` contains additional development assets