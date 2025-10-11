# Chart Preview TODO & Roadmap
# Consolidated master list - architectural + beta tester feedback

Current Version: **v0.9.0 (Hit animations & REAPER integration)**
Primary Beta Tester: **Invontor** (main contributor/QA)

**NOTE**: See `UPCOMING_FEATURES.md` for detailed implementation plan of new priority items.

## 🚨 CRITICAL BUGS (P0) - Beta Blockers

### Guitar Playability Issues
- ✅ ~~Fix force strum/hopo markers applying to all notes underneath~~ - **COMPLETED** (replaced tolerance checks with exact matching)
- ✅ ~~Fix natural hopos incorrectly applying to chords~~ - **COMPLETED** (added retroactive chord HOPO fixing)
- ✅ ~~Fix drum tom markers applying to all notes underneath~~ - **COMPLETED** (generalized sustained modifier processing)
- ✅ ~~Fix OD should apply to whole sustain even when phrase doesn't fully cover it~~ - **COMPLETED** (improved MIDI cleanup preservation)
- ✅ ~~Fix sustains under minimum threshold not being clipped~~ - **COMPLETED** (now uses proper MIN_SUSTAIN_LENGTH threshold)
- ✅ ~~Fix sustain length/endings rendering inaccurately~~ - **COMPLETED** (fixed with proper threshold)
- ✅ ~~Fix force strum/hopo markers loading/applying inconsistently during playback~~ - **COMPLETED** (fixed by sustained modifier processing order + cleanup preservation)

### Architecture & Threading
- ✅ ~~Fix race condition causing intermittent black screens during playback~~ - **COMPLETED** (atomic clear+write operations in REAPER pipeline)
- ✅ ~~Optimize debug logging performance~~ - **COMPLETED** (conditional logging based on debug toggle state)
- 🔄 **Make renderer data access thread-safe** (has gridlineMapLock, needs double-buffered snapshot)
- ❌ **Migrate parameters to AudioProcessorValueTreeState** and wire UI bindings
- 🔄 **Audio-thread hygiene**: eliminated duplicate generateTrackWindow calls; still need to remove std::function in draw path and use preallocated vectors

### REAPER Mode Rendering Issues
- ✅ ~~Fix bar/measure line positioning accuracy (REAPER mode)~~ - **COMPLETED** (fixed with time signature change handling)
- ✅ ~~Fix tempo-change stretching (non-dynamic zoom)~~ - **COMPLETED** (refactored to time-based system with unified tempo handling)

### Sync/Timing Issues
- ✅ ~~Fix preview sync issue occurring between plugin restarts/reloads~~ - **COMPLETED** (fixed by absolute position-based rendering)
- ✅ ~~Fix notes rendering early~~ - **COMPLETED** (fixed by absolute position-based rendering)

### Plugin Loading
- ✅ ~~Fix plugin loading failure in REAPER v7.43~~ - **RESOLVED** (likely fixed by architecture improvements, no longer reproducible)

### Host Integration
- ❌ **Host fallback UX** when no playhead or not playing (has basic fallback, needs comprehensive UX)

## 🔥 HIGH PRIORITY (P1) - Core Features & UX

### Feature Completion
- ✅ ~~Guitar sustains (render + hit windows)~~ - **COMPLETED in v0.8.5**
- ✅ ~~Drums lanes~~ - **COMPLETED in v0.8.5** (lanes implemented)
- ❌ **BRE**
- ❌ **Extended memory** so notes remain visible when transport stops
- ✅ ~~Latency compensation and user calibration control~~ - **COMPLETED**
- ✅ ~~Finalize grid visual polish~~ - **COMPLETED**

### Default Settings & UX Polish
- ✅ ~~Add version number display in plugin UI~~ - **COMPLETED** (v0.8.7)
- ❌ **Add REAPER mode toggle (click logo)** - Enable/disable REAPER timeline mode
- ❌ **Make 170 ticks the default auto-HOPO setting** (most accurate for modern games)
- ❌ **Make HOPO settings "advanced" section**, hide complexity from beginners
- ❌ **Make kicks wider to go to edges** (visual consistency with other games)
- ❌ **Fix window resizing not preserved** across project loads/Reaper restarts

### Missing Core Features
- ✅ ~~Re-enable lanes rendering~~ - **COMPLETED in v0.8.5** (fixed rounded corner rendering with offscreen compositing)
- ✅ ~~Add hit animation/effects~~ - **COMPLETED in v0.9.0** (per-column hit indicators with 17 new animation frames)
- ❌ **Add time offset setting/slider** for manual audio sync compensation

## 📋 MEDIUM PRIORITY (P2) - Polish & Performance

### Reaper Integration
- ❌ **Implement Reaper plugin preset system integration**
      - Save parameters to .ini files in Reaper directory
      - Interface properly with JUCE parameter system
- ❌ **Add fixed scroll speed option** (vs tempo-based)
- ❌ **Add save custom settings/profile system**

### Performance & Rendering
- 🔄 **Draw-call optimizations** (eliminated duplicate work, still need culling, layer batching, image pre-scaling)
- ❌ **Metronome click playback** aligned to PPQ (see UPCOMING_FEATURES.md)
- ❌ **Guitar note click playback** (see UPCOMING_FEATURES.md)
- ❌ **Drum sample playback** aligned to PPQ (see UPCOMING_FEATURES.md)
- ❌ **FPS/cpu overlay toggle** and targeted logging overlay for debugging
- ❌ **DPI scaling, asset validation, fallback artwork**

### Visual Improvements
- ❌ **Add visual tempo/time signature change markers** (see UPCOMING_FEATURES.md)
- ❌ **Visual cleanup** - note sizing and beat line improvements
- ❌ **Fix thicker beat lines inconsistently rendered** in wrong spots
- ❌ **Fix window occasionally allowing asymmetric scaling** (aspect ratio bug)

## 🚀 FUTURE FEATURES (P3) - Advanced

### Advanced Instrument Support
- ❌ **Pro Guitar/Bass support** (6-string) - significant development effort, gauge interest
- ❌ **5-lane drums** (Guitar Hero style)
- ❌ **Real Drums support** (pro drums)

### HOPO & Advanced Features  
- ❌ **Heuristic HOPOs, open HOPOs** *(partially implemented - auto-HOPO system exists)*
- ✅ ~~Trill/tremolo sections~~ - **COMPLETED in v0.8.5** (lanes system with rounded rendering)
- ❌ **Face off/Battle mode sections support**

### Advanced Workflow
- ❌ **Automatic REAPER track detection** (see UPCOMING_FEATURES.md)
- ❌ **MIDI lookahead (REAPER SWS)** with clean fallback elsewhere
- ❌ **Guitar/bass hand animation preview** with visual reference
- ❌ **Drum hand animation preview support**
- ❌ **Light show preview** (GH3/RB style) - color stage lighting based on notes
- ❌ **Audio wave preview inside highway** (like Moonscraper inline audio)
- ❌ **Higher FPS caps** (240fps, 360fps for ultra-high refresh displays)

### Multi-track Features
- ❌ **Preview different difficulties while editing another track**
- ❌ **Preview different instruments simultaneously**  
- ❌ **Custom MIDI channel editor** with audio track background

## ✅ RECENT COMPLETIONS

### v0.9.0 - Hit Animations & REAPER Integration
- ✅ **Hit Animation System** - Per-column hit indicators with kick bar flashes (17 new animation frames)
- ✅ **Hit Indicators Toggle** - Show/hide hit animations per user preference
- ✅ **Speed Slider** - Renamed from "zoom" for UI clarity
- ✅ **Gridline Alignment** - Adjusted positioning to align with gem notes
- ✅ **isKick→isBar Terminology** - Consistent naming across codebase
- ✅ **REAPER Timeline Integration** - Direct MIDI access via VST2/VST3 extensions
- ✅ **Modular Pipeline Architecture** - ReaperMidiPipeline vs StandardMidiPipeline
- ✅ **MIDI Caching System** - Smart caching with invalidation on track switching
- ✅ **Multi-instance Support** - Per-instance API storage
- ✅ **Time-based Rendering** - Refactored to unified tempo handling (absolute position-based)
- ✅ **REAPER Gridline Alignment** - Fixed with time signature change handling
- ✅ **Tempo-change Stretching Fix** - No more jarring visual stretching during tempo changes
- ✅ **Chord HOPO Rendering** - Fixed bug in both MIDI pipelines
- ✅ **Force Rendering** - Every frame for smooth visual updates
- ✅ **Centralized Debug Logging** - DebugTools::Logger system

### v0.8.7 - REAPER Optimization Update
- ✅ **Version Display** - Added to plugin UI
- ✅ **Sync Issues Fixed** - Absolute position-based rendering eliminates timing inconsistencies

### v0.8.6
- ✅ **Race condition fix** - Atomic clear+write operations preventing intermittent black screens
- ✅ **Debug logging optimization** - Conditional logging based on debug toggle state

### v0.8.5
- ✅ Convert all note timing/state to PPQ and render from PPQ
- ✅ Gridlines: PPQ-based beats/measures with subdivisions
- ✅ Latency compensation and user calibration control with multi-buffer smoothing
- ✅ Grid visual polish (beat/half-beat/measure markers, visibility working)  
- ✅ CI/release automation across platforms (GitHub Actions with Windows/macOS/Linux builds)
- ✅ **Lanes System Overhaul** - Complete rewrite with perspective-aware coordinates, rounded cap rendering, and offscreen compositing
- ✅ **Sustain Visual Polish** - Added rounded corners, proportional perspective scaling, and pixel-perfect rendering
- ✅ **Coordinate System Refactor** - Dedicated lane coordinate functions independent of gem positioning for consistent spacing
- ✅ Sustain note implementation
- ✅ Resizable VST with fixed aspect ratio  
- ✅ Chord detection tolerance (10-tick grouping)
- ✅ Drum/cymbal lanes (tremolo/trills) implementation
- ✅ HOPO mode configuration system

## 🎯 IMMEDIATE NEXT STEPS (Priority Order)

**See `UPCOMING_FEATURES.md` for detailed implementation plan.**

### Phase 1: UX Polish (v0.9.x - v0.10.0)
1. **Add REAPER mode toggle** (click logo) - Enable/disable REAPER timeline mode
2. **Make 170 ticks default auto-HOPO** + make HOPO settings "advanced" section
3. **Make kicks wider** to go to edges
4. ✅ ~~Add hit animation/effects~~ - **COMPLETED in v0.9.0**

### Phase 2: Performance & Architecture
5. **Complete renderer thread safety** (double-buffered snapshot)
6. **Audio-thread hygiene** (remove std::function, preallocated vectors)
7. **Migrate to AudioProcessorValueTreeState**

### Phase 3: Audio Features (Future)
8. **Metronome playback** (2-3 days)
9. **Guitar click playback** (1 day)
10. **Drum sample playback** (3-4 days)
11. **Visual tempo/time sig markers** (1 day)

### Phase 4: Advanced Features (Future)
12. **BRE support**
13. **Extended memory** (notes remain visible when stopped)
14. **Auto track detection** (REAPER, 3-5 days research heavy)

## 📝 FEEDBACK SOURCES & STATUS
- **Invontor** (primary beta tester, main contributor) - "Drums fully usable with quirks, Guitar less usable due to bugs"
- **feather [YARG]** (plugin loading issues in v0.7)
- **John Smith [MHX]** (Pro Guitar interest, hit animation feedback)
- **Roney_Nero_149 [MHX]** (advanced feature requests)
- **goulart [YARG]** (auto-HOPO questions)
- **maria [ENCR]** (Linux support interest)

**Legend:** ✅ Complete | 🔄 In Progress | ❌ Not Started

Last Updated: 2025-10-11 (v0.9.0 release)