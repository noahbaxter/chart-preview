# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## IMPORTANT: Git Commit Guidelines

**NEVER add co-authoring or "Generated with Claude Code" to commit messages unless explicitly requested by the user.**

When creating commits, use ONLY the user's requested message format without any additional attribution or co-authoring lines.

## Project Overview

Chart Preview is a VST/AU plugin for DAWs that visualizes MIDI notes as rhythm game charts (similar to Clone Hero/YARG). It's built with JUCE framework and designed for Windows, macOS, and Linux platforms.

## Development Commands

### Building the Plugin

**Cross-Platform CI/CD (Automated)**
GitHub Actions automatically builds for all platforms on push to any branch:
- **Windows**: VST3 plugin (optimized, ~2.5MB)
- **macOS**: Universal binary VST3 + AU (x86_64 + arm64, ~6.7MB) 
- **Linux**: VST3 plugin (~4.4MB)

Artifacts are automatically generated and downloadable from GitHub Actions.

**Local Development**

**macOS (Primary Development)**
```bash
cd ChartPreview
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
Use Visual Studio 2022 project at `ChartPreview/Builds/VisualStudio2022/ChartPreview.sln`

**Linux**
```bash
cd ChartPreview
chmod +x build-linux.sh
./build-linux.sh
```

Or manually:
```bash
cd ChartPreview/Builds/LinuxMakefile
make -j$(nproc) CONFIG=Release
```

Requirements: JUCE 8.0.0, system dependencies (libcurl, libfreetype, etc.)

### Projucer Integration

The main project file is `ChartPreview/ChartPreview.jucer`. When modified:
- The build script automatically regenerates platform projects
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

- Graphics assets in `ChartPreview/Assets/`
- Audio assets in `ChartPreview/Audio/`
- Assets are embedded in compiled binaries (verified in CI)
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

Current version: **v0.7+ (beta testing phase)**

See `docs/TODO.md` for comprehensive priority-ranked backlog based on beta tester feedback.

### Recent Completions (v0.7)
- ✅ PPQ-based timing system conversion
- ✅ Latency compensation with multi-buffer smoothing
- ✅ Grid visual polish (beat/half-beat/measure markers)
- ✅ Cross-platform CI/CD pipeline with artifact verification
- ✅ Linux build support with full dependency management
- ✅ Windows artifact optimization (debug symbol removal)
- ✅ Sustain note implementation
- ✅ Resizable VST with fixed aspect ratio
- ✅ Chord detection tolerance (10-tick grouping)
- ✅ Drum/cymbal lanes (tremolo/trills) implementation
- ✅ HOPO mode configuration system

### Beta Testing Insights

**Primary Tester:** Invontor (main contributor/QA)  
**Current Status:** Drums fully usable with quirks, Guitar less usable due to bugs

**Critical Issues Identified:**
- Force strum/HOPO markers only apply to first note (should cover all notes underneath)
- Sync issues varying between plugin restarts/instances
- Sustain length rendering inaccuracies
- Plugin loading failures in some Reaper versions

**User Experience Feedback:**
- Default HOPO setting should be 170 ticks (most accurate for modern games)
- Kicks should be wider to match other games' visual conventions
- Hit animations/effects needed (users find lack of feedback jarring)
- Window sizing not persistent across project loads

### Current Priorities
- **P0 (Critical)**: Fix force marker coverage, sync consistency, plugin loading
- **P1 (High)**: Default settings improvements, visual polish, hit animations  
- **P2 (Medium)**: Reaper integration, performance optimization
- **P3 (Future)**: Advanced features (Pro Guitar, Real Drums, etc.)

## File Structure Notes

- Main source in `ChartPreview/Source/`
- Platform-specific builds in `ChartPreview/Builds/`
  - `MacOSX/`: Xcode project files
  - `VisualStudio2022/`: Visual Studio solution and projects  
  - `LinuxMakefile/`: Linux build system and dependencies
- Asset files in `ChartPreview/Assets/` (embedded in binaries)
- Audio assets in `ChartPreview/Audio/` (embedded in binaries)
- CI/CD configuration in `.github/workflows/build.yml`
- `DO NOT DISTRIBUTE/` contains additional development assets

## Build System Details

### CI/CD Pipeline
- **Triggers**: Any push to any branch, all pull requests
- **Platforms**: Windows Server 2022, macOS 14, Ubuntu 22.04
- **Optimization**: Windows artifacts exclude debug symbols (~92% size reduction)
- **Verification**: Automatic validation of embedded resources and dependencies
- **Caching**: JUCE installations and build artifacts cached for faster builds
- **Distribution**: Downloadable artifacts for each platform automatically generated

### Platform-Specific Notes
- **Windows**: Uses MSBuild with Visual Studio 2022, produces optimized VST3 DLL
- **macOS**: Uses Xcode with universal binary support, produces both VST3 and AU
- **Linux**: Uses Make with system package dependencies, produces VST3 bundle