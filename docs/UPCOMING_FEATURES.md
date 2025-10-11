# Upcoming Features & Roadmap
# Priority-ordered implementation plan for v0.9.0+

Current Version: **v0.9.0 (Hit animations & REAPER integration)**
Last Updated: 2025-10-11 (v0.9.0 release)

---

## ✅ **COMPLETED IN v0.9.0**

### Hit Animation System ✅ DONE
**Completed**: v0.9.0

**Implemented**:
- ✅ Per-column hit indicators with 17 new animation frames
- ✅ Kick bar flashes synchronized to drum hits
- ✅ Hit indicators toggle button
- ✅ Frame-based animation timing system
- ✅ Gridline alignment adjustments
- ✅ Speed slider (renamed from zoom for clarity)

### REAPER Integration ✅ DONE
**Completed**: v0.8.7→v0.9.0

**Implemented**:
- ✅ REAPER timeline integration via VST2/VST3 extensions
- ✅ Modular pipeline architecture (ReaperMidiPipeline vs StandardMidiPipeline)
- ✅ MIDI caching system with smart invalidation
- ✅ Multi-instance support
- ✅ Time-based rendering with unified tempo handling
- ✅ Centralized debug logging system

### Rendering Fixes ✅ DONE
**Completed**: v0.8.7→v0.9.0

**Fixed**:
- ✅ Bar/measure line positioning (fixed with time signature change handling)
- ✅ Tempo-change stretching (refactored to time-based system)
- ✅ Sync timing issues (fixed by absolute position-based rendering)
- ✅ Gridline alignment with gem notes
- ✅ Version number display in UI

---

## 🎯 **Tier 1 - Quick Wins (Immediate Impact)**

### 2. REAPER Mode Toggle (Click Logo) ⚡ EASY
**Status**: Not started
**Effort**: 2-3 hours
**Priority**: P1 - UX improvement

**Description**: Make the REAPER logo clickable to toggle between REAPER mode (timeline API) and standard buffer mode. Should:
- Only be available when hosted in REAPER
- Enable REAPER mode by default when in REAPER
- Allow users to switch to buffer mode if preferred
- Persist state across sessions

**Impact**:
- Removes "hacky" manual track selection requirement
- Enables testing buffer mode within REAPER
- Better user control and flexibility

**Technical Notes**:
- Add click handler to reaperLogo drawable
- Add `reaperModeEnabled` to state ValueTree
- Auto-detect REAPER host on plugin initialization
- Show/hide track selector based on mode

---

## 🔥 **Tier 2 - Visual Polish (High Impact)**

### 3. Visual Tempo/Time Signature Change Markers 📊 MEDIUM
**Status**: Not started
**Effort**: 1 day
**Priority**: P2 - Quality of life

**Description**: Add visual markers at every tempo/time signature change point on the highway. Should be visible but not too prominent when many changes occur in quick succession.

**Design Considerations**:
- Subtle marker (colored line or icon?)
- Opacity scaling based on density (fade when many markers close together)
- Show BPM/time sig value on hover or always?
- Color code: tempo changes vs time sig changes

**Impact**:
- Helps users understand what's happening during tempo changes
- Makes charting workflow clearer
- Good complement to the existing tempo-aware rendering

**Technical Notes**:
- Render during gridline pass
- Use TempoMap and TimeSignatureMap from #3/#4
- Draw vertical line or marker at change points
- Add label rendering (optional)

---

## 🎵 **Tier 3 - Audio Features (Nice to Have)**

### 4. Metronome Click Playback 🔊 MEDIUM
**Status**: Not started
**Effort**: 2-3 days
**Priority**: P2 - Workflow improvement

**Description**: Add audio click track that plays on the beat, synchronized to the chart grid. Helps users verify timing and alignment.

**Features**:
- Toggle on/off
- Volume control
- Accent on measure boundaries
- PPQ-based scheduling (tempo-aware)

**Impact**:
- Helps verify chart alignment with audio
- Useful for charting without full song playback
- Standard feature in DAWs/chart editors

**Technical Notes**:
- Generate click samples or use embedded audio
- Schedule based on GridlineMap beat positions
- Render audio in `processBlock()` alongside MIDI
- Need to handle latency compensation

---

### 5. Guitar Note Click Playback 🔊 MEDIUM
**Status**: Not started
**Effort**: 1 day (reuses metronome infrastructure)
**Priority**: P2 - Workflow improvement

**Description**: Play click sound when guitar notes appear on the highway. Helps verify note timing and placement.

**Features**:
- Toggle on/off (separate from metronome)
- Volume control
- Different sounds per fret/open note (optional)
- Respects HOPO/strum distinctions (optional)

**Impact**:
- Improves guitar charting workflow
- Helps identify timing issues
- Useful for charts without full audio

**Dependencies**: Requires metronome infrastructure (#4)

---

### 6. Drum Sample Playback 🔊 HIGH EFFORT
**Status**: Not started
**Effort**: 3-4 days
**Priority**: P2 - Workflow improvement

**Description**: Play drum samples when drum notes appear on the highway. Each lane triggers appropriate drum sound (kick, snare, toms, cymbals).

**Features**:
- Toggle on/off
- Volume control
- Sample library for each drum type
- Velocity-sensitive playback (optional)
- Cymbal/tom differentiation
- Ghost note handling

**Impact**:
- Significantly improves drum charting workflow
- Helps verify drum arrangement sounds correct
- Makes silent charting sessions more productive

**Technical Notes**:
- Need drum sample library (kick, snare, hi-hat, crash, ride, toms)
- Multi-voice sample playback (handle simultaneous hits)
- Map MIDI pitches to sample types
- Consider using existing Audio/ folder for samples
- May need sample mixing/voice stealing for dense charts

**Dependencies**: Requires metronome infrastructure (#4)

---

## 🔧 **Tier 4 - Advanced Integration (Complex)**

### 7. Automatic REAPER Track Detection 🔬 COMPLEX
**Status**: Research needed
**Effort**: 3-5 days (research heavy)
**Priority**: P2 - UX improvement

**Description**: Automatically detect which track the plugin instance is on and read MIDI from that track. Should update automatically when plugin is moved to a different track.

**Current Behavior**:
- User must manually select track number from dropdown
- Feels "hacky" and error-prone
- Doesn't update when plugin is moved

**Desired Behavior**:
- Plugin automatically knows which track it's on
- Updates when moved to different track
- No manual configuration required

**Research Needed**:
- REAPER extension API capabilities
- How to query track number for current FX instance
- How to detect track changes/moves
- Whether this requires REAPER extension or can use existing API

**Challenges**:
- May require REAPER-specific extension APIs
- Need fallback if API not available
- Handle edge cases (moved tracks, renamed tracks, frozen tracks)
- May need to poll for changes vs event-based detection

**Impact**:
- Excellent UX improvement
- Removes manual configuration step
- Makes plugin feel more integrated

**Technical Notes**:
- Investigate `ReaperMidiProvider` capabilities
- May need `GetTrackFromFX` or similar REAPER API
- Add track change detection polling
- Preserve manual override option for edge cases

---

## 📋 **Implementation Timeline**

### ✅ Phase 1: REAPER Foundation (v0.8.7→v0.9.0) - COMPLETE
- ✅ Version number display
- ✅ REAPER timeline integration
- ✅ Modular pipeline architecture
- ✅ Time-based rendering refactor
- ✅ Bar/measure positioning fix
- ✅ Tempo-change stretching fix
- ✅ Sync issues fix
- ✅ Hit animations with 17 new frames
- ✅ Gridline alignment improvements
- ✅ Speed slider UI clarity

### Phase 2: UX Polish (v0.9.x→v0.10.0)
- REAPER mode toggle (2-3 hours)
- Default HOPO 170 ticks + advanced settings
- Wider kicks

### Phase 3: Audio Features (v0.10.0+)
- Metronome playback (2-3 days)
- Guitar click playback (1 day)
- Drum sample playback (3-4 days)
- Tempo/time sig markers (1 day)

### Phase 4: Advanced Integration (Future)
- Auto track detection (3-5 days, research heavy)
- BRE support
- Extended memory

---

## 🎯 **v0.9.0 Achievement Summary**

The hit animations and REAPER integration update is **COMPLETE**! All critical rendering, timing, and hit feedback issues have been resolved:

- ✅ **Hit animations live** - 17 new animation frames with per-column indicators
- ✅ **Visual accuracy restored** - Bar positioning and tempo changes work correctly
- ✅ **Sync issues eliminated** - Absolute position-based rendering
- ✅ **REAPER integration solid** - Direct timeline access, caching, multi-instance support
- ✅ **Architecture modernized** - Modular pipelines, thread safety improvements
- ✅ **UI clarity improved** - Speed slider renamed, gridlines aligned

**Next focus**: UX polish (mode toggle, defaults, wider kicks) for v0.10.0

---

## 📝 **Notes**

- All features should preserve backward compatibility with existing state
- Audio features (#6-8) need audio asset management
- REAPER-specific features (#2, #3, #4, #9) should degrade gracefully in other hosts
- Consider adding feature flags for beta testing individual features
