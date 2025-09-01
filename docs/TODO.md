# Chart Preview TODO & Roadmap
# Consolidated master list - architectural + beta tester feedback

Current Version: **v0.7+ (beta testing phase)**  
Primary Beta Tester: **Invontor** (main contributor/QA)

## 🚨 CRITICAL BUGS (P0) - Beta Blockers

### Guitar Playability Issues
- ❌ **Fix force strum/hopo markers applying to all notes underneath** (not just first note)
      - Currently only applies to first note under marker, should work like OD phrases
      - Same logic applies to drum tom markers - MAJOR usability blocker
- ❌ **Fix natural hopos incorrectly applying to chords** 
- ❌ **Fix force strum/hopo markers loading/applying inconsistently during playback**
- ❌ **Fix sustain length/endings rendering inaccurately** (appear too long)
- ❌ **Fix sustains under minimum threshold not being clipped** (baby sustains display)
- ❌ **Fix OD should apply to whole sustain even when phrase doesn't fully cover it**

### Architecture & Threading
- 🔄 **Make renderer data access thread-safe** (has gridlineMapLock, needs double-buffered snapshot)
- ❌ **Migrate parameters to AudioProcessorValueTreeState** and wire UI bindings
- 🔄 **Audio-thread hygiene**: eliminated duplicate generateTrackWindow calls; still need to remove std::function in draw path and use preallocated vectors

### Sync/Timing Issues  
- ❌ **Fix preview sync issue occurring between plugin restarts/reloads**
      - Inconsistent between separate plugin instances after single restart
      - Some users report 300ms early, others vary wildly - MAJOR usability blocker
- ❌ **Fix notes rendering early** (nearly a whole 16th note early)
      - Evident when beatlines visible between fast kicks/tremolo strumming
- ❌ **Fix off-grid rendering around tempo changes**
      - Beat markers vs notes due to latency compensation mismatch

### Plugin Loading
- ❌ **Fix plugin loading failure in Reaper v7.43** for some users
      - Works in v0.5/0.6, fails in v0.7
      - Shows in "plugins that failed to scan" section

### Host Integration
- ❌ **Host fallback UX** when no playhead or not playing (has basic fallback, needs comprehensive UX)

## 🔥 HIGH PRIORITY (P1) - Core Features & UX

### Feature Completion
- ✅ ~~Guitar sustains (render + hit windows)~~ - **COMPLETED in v0.7**
- ✅ ~~Drums lanes + BRE~~ - **COMPLETED in v0.7** (lanes implemented, BRE pending)
- ❌ **Extended memory** so notes remain visible when transport stops
- ✅ ~~Latency compensation and user calibration control~~ - **COMPLETED**
- ✅ ~~Finalize grid visual polish~~ - **COMPLETED**

### Default Settings & UX Polish
- ❌ **Make 170 ticks the default auto-HOPO setting** (most accurate for modern games)
- ❌ **Make HOPO settings "advanced" section**, hide complexity from beginners  
- ❌ **Add version number display in plugin UI**
- ❌ **Make kicks wider to go to edges** (visual consistency with other games)
- ❌ **Fix window resizing not preserved** across project loads/Reaper restarts

### Missing Core Features
- ❌ **Re-enable lanes rendering** (currently disabled due to visual issues)
      - Need to fix rounded corner rendering problems from recent work
- ❌ **Add hit animation/effects** (light flash) with toggle option
      - Users report jarring lack of hit feedback compared to RBN preview - HIGH DEMAND
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
- ❌ **Metronome click playback** and drum sample playback aligned to PPQ
- ❌ **FPS/cpu overlay toggle** and targeted logging overlay for debugging
- ❌ **DPI scaling, asset validation, fallback artwork**

### Visual Improvements  
- ❌ **Visual cleanup** - note sizing and beat line improvements
- ❌ **Fix thicker beat lines inconsistently rendered** in wrong spots
- ❌ **Better measure line accuracy** (not in line with measure starts)
- ❌ **Fix window occasionally allowing asymmetric scaling** (aspect ratio bug)

### Memory & Timing Architecture
- ❌ **Build memory map of time signature changes**
      - Fix bar line issues when starting playback mid-measure
      - Remove dependency on knowing last time sig from current buffer

## 🚀 FUTURE FEATURES (P3) - Advanced

### Advanced Instrument Support
- ❌ **Pro Guitar/Bass support** (6-string) - significant development effort, gauge interest
- ❌ **5-lane drums** (Guitar Hero style)
- ❌ **Real Drums support** (pro drums)

### HOPO & Advanced Features  
- ❌ **Heuristic HOPOs, open HOPOs** *(partially implemented - auto-HOPO system exists)*
- ❌ **Trill/tremolo sections** *(lanes system completed, needs re-enabling)*
- ❌ **Face off/Battle mode sections support**

### Advanced Workflow
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

## ✅ RECENT COMPLETIONS (v0.7)
- ✅ Convert all note timing/state to PPQ and render from PPQ
- ✅ Gridlines: PPQ-based beats/measures with subdivisions
- ✅ Latency compensation and user calibration control with multi-buffer smoothing
- ✅ Grid visual polish (beat/half-beat/measure markers, visibility working)  
- ✅ CI/release automation across platforms (GitHub Actions with Windows/macOS/Linux builds)
- ✅ Sustain note implementation
- ✅ Resizable VST with fixed aspect ratio  
- ✅ Chord detection tolerance (10-tick grouping)
- ✅ Drum/cymbal lanes (tremolo/trills) implementation
- ✅ HOPO mode configuration system

## 🎯 IMMEDIATE NEXT STEPS (Priority Order)
1. **Fix force marker coverage bug** (blocks guitar usability)
2. **Fix sync consistency issues** (major usability blocker)  
3. **Fix plugin loading in Reaper 7.43**
4. **Default HOPO to 170 ticks + make settings advanced**
5. **Add version display**
6. **Make kicks wider**
7. **Add hit animations** (high user demand)

## 📝 FEEDBACK SOURCES & STATUS
- **Invontor** (primary beta tester, main contributor) - "Drums fully usable with quirks, Guitar less usable due to bugs"
- **feather [YARG]** (plugin loading issues in v0.7)
- **John Smith [MHX]** (Pro Guitar interest, hit animation feedback)
- **Roney_Nero_149 [MHX]** (advanced feature requests)
- **goulart [YARG]** (auto-HOPO questions)
- **maria [ENCR]** (Linux support interest)

**Legend:** ✅ Complete | 🔄 In Progress | ❌ Not Started

Last Updated: 2025-09-01