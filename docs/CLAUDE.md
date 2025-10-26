# CLAUDE.md

Guidance for Claude Code when working with this repository.

## IMPORTANT: Git Commit Guidelines

**NEVER add co-authoring or "Generated with Claude Code" to commit messages unless explicitly requested.**

## CRITICAL: Use Local JUCE Submodule Only

**DO NOT reference /Applications/JUCE** - This project uses a custom JUCE submodule with REAPER modifications.

**ALWAYS use**: `third_party/JUCE/` for all JUCE source code references.

When researching JUCE implementation details:
1. Check `third_party/JUCE/modules/` first
2. Refer to `/docs/` for project-specific documentation
3. Only use web searches if local resources are insufficient
4. Document findings in `/docs/` for future reference

## Project Overview

Chart Preview is a VST/AU plugin that visualizes MIDI notes as rhythm game charts (Clone Hero/YARG style). Built with JUCE for Windows, macOS, and Linux.

## Development Commands

### Building the Plugin

**CI/CD**: GitHub Actions builds all platforms automatically (Windows VST3 ~2.5MB, macOS universal VST3+AU ~6.7MB, Linux VST3 ~4.4MB)

**Local REAPER Testing** (macOS): `cd ChartPreview && ./build-scripts/build-local-test.sh [--open-reaper]`
  - Builds VST2 + VST3 in Debug mode
  - Installs to plugin folders
  - Use `--open-reaper` flag to force quit & reopen REAPER
  - For local testing only - doesn't affect CI

**macOS Release**: `./build-scripts/build-macos-release.sh`
**Windows**: Use `ChartPreview/Builds/VisualStudio2022/ChartPreview.sln`
**Linux**: `./build-linux.sh` or `make -j$(nproc) CONFIG=Release` in `Builds/LinuxMakefile/`

### Projucer
Main file: `ChartPreview.jucer`. Build script auto-regenerates platform projects, or manual: `/Applications/JUCE/Projucer.app --resave`

### Testing
REAPER: `examples/reaper/`, Ableton: `examples/ableton/`

## Architecture

### Core Components

**Audio Processing Chain:**
1. `ChartPreviewAudioProcessor` - Main plugin processor
2. **REAPER Mode**: `ReaperMidiPipeline` - Direct timeline MIDI access via REAPER API
3. **Standard Mode**: `MidiProcessor` - Processes MIDI buffer events
4. `MidiInterpreter` - Converts MIDI data to visual chart elements
5. `HighwayRenderer` - Renders the chart visualization
6. `ChartPreviewAudioProcessorEditor` - UI and real-time display

**Key Data Structures:**
- `NoteStateMapArray` - Tracks held notes across all MIDI pitches (thread-safe)
- `TrackWindow` - Frame-based chart data for rendering window
- `SustainWindow` - Sustain note data for rendering window
- `GridlineMap` - Beat/measure grid timing information (thread-safe)
- `MidiCache` - Cached REAPER timeline MIDI data with smart invalidation

### Threading & Race Condition Prevention
**CRITICAL**: Audio thread processes MIDI, GUI thread renders. Thread-safe with `juce::CriticalSection`.

**Race Condition Fix (v0.8.6)**: `ReaperMidiPipeline::processCachedNotesIntoState` holds `noteStateMapLock` for the ENTIRE clear+write operation. This prevents the renderer from reading an empty `noteStateMapArray` between the clear and write steps (which caused intermittent black screens).

**Pattern**: Always use atomic clear+write when updating shared data structures read by the rendering thread.

### Timing
All timing uses **PPQ (Pulses Per Quarter)** for tempo-independence.

### Instruments
**Drums**: Normal/Pro modes, ghost notes, accents, 2x kick, cymbal/tom differentiation, lanes
**Guitar**: Open notes, tap notes, HOPOs (configurable), sustains, lanes

## Important Implementation Details

### MIDI Pitch Mappings

Defined in `Utils.h` as `MidiPitchDefinitions`:
- Separate mappings for Guitar and Drums
- Multiple difficulty levels (Easy, Medium, Hard, Expert)
- Special pitches for star power, dynamics, lanes

### Assets & Performance
- Assets in `Assets/` and `Audio/`, embedded in binaries
- 60 FPS default (configurable: 15/30/60/120/144)
- Latency compensation with multi-buffer smoothing
- Draw call batching, offscreen compositing, perspective-aware caching
- **Debug logging optimization**: Conditional string concatenation only when debug console is open (prevents performance degradation)

## State Management

Plugin state stored in `juce::ValueTree` with properties:
- `part`: Guitar/Drums selection
- `skillLevel`: Easy/Medium/Hard/Expert
- `framerate`: 15/30/60 FPS
- `latency`: 250ms/500ms/750ms/1000ms/1500ms
- `starPower`, `kick2x`, `dynamics`: Visibility toggles

## Development Status

**Current**: v0.9.4 (Hit animations & REAPER integration)
**Next**: v0.9.5 (Quick wins) → v1.0.0 (Core features + UI overhaul)
**See**: `docs/TODO.md` for current roadmap and priorities

### Recent Completions (v0.9.x)
- Hit animations (17 new animation frames)
- REAPER timeline integration via VST2/VST3 extensions
- Modular pipeline architecture (ReaperMidiPipeline vs StandardMidiPipeline)
- MIDI caching system with smart invalidation
- Multi-instance support with per-instance API storage
- Time-based rendering refactor with unified tempo handling
- REAPER gridline alignment fixed with time signature change handling
- Centralized debug logging system (DebugTools::Logger)

### Beta Testing (Invontor - main tester)
**Status**: Core functionality solid, ready for UX polish phase

**Completed Fixes**: All critical rendering issues (bar positioning, tempo stretching, sync timing)
**UX Feedback**: REAPER mode toggle, default HOPO 170 ticks, wider kicks, hit animations, persistent window sizing

**Current Focus**: UX polish (mode toggle, defaults, animations) → Audio features (metronome, clicks, samples) → Advanced features (BRE, extended memory)

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

## Build System

**CI/CD**: Triggers on all pushes/PRs. Windows (VS2022, optimized), macOS (Xcode, universal VST3+AU), Linux (Make, VST3). Artifacts cached and auto-generated.