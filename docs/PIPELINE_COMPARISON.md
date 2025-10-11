# Pipeline Feature Comparison
# REAPER Pipeline vs Standard Pipeline

Last Updated: 2025-10-10

---

## 🎯 **Quick Overview**

Chart Preview uses two different MIDI processing pipelines depending on the host:

- **REAPER Pipeline** (`ReaperMidiPipeline`) - Direct timeline access via REAPER API
- **Standard Pipeline** (`StandardMidiPipeline`) - Traditional VST buffer-based processing

---

## 📊 **Feature Parity Matrix**

| Feature | REAPER Pipeline | Standard Pipeline | Notes |
|---------|----------------|-------------------|-------|
| **Core Features** |
| Note rendering | ✅ | ✅ | Both support all note types |
| Sustain rendering | ✅ | ✅ | Full sustain support |
| HOPO detection | ✅ | ✅ | Auto + forced HOPOs |
| Chord detection | ✅ | ✅ | 10-tick tolerance |
| Gridlines (beats/measures) | ✅ | ✅ | PPQ-based |
| Star power | ✅ | ✅ | Phrases + individual notes |
| Lanes (trills/tremolo) | ✅ | ✅ | Guitar + drums |
| **Timing & Sync** |
| Real-time playback | ✅ | ✅ | Both support live playback |
| Scrubbing support | ✅ | ⚠️ Limited | REAPER: full scrub support<br>Standard: depends on host buffer behavior |
| Tempo changes | ✅ | ✅ | Absolute position-based rendering |
| Time signature changes | ✅ | ✅ | Both handle correctly |
| Latency compensation | ✅ | ✅ | User-configurable |
| **Data Access** |
| MIDI lookahead | ✅ Native | ⚠️ Host-dependent | REAPER: direct timeline access<br>Standard: limited to buffer size |
| Timeline position | ✅ Absolute | ✅ Relative | REAPER: PPQ from project start<br>Standard: from playback start |
| Track selection | ✅ Manual | N/A | REAPER: dropdown to select track<br>Standard: reads current track |
| Multi-track access | ✅ | ❌ | REAPER: can read any track<br>Standard: only receives current track MIDI |
| **Performance** |
| MIDI caching | ✅ | ❌ | REAPER: smart cache with invalidation<br>Standard: processes buffer each frame |
| Cache invalidation | ✅ | N/A | Track switching, project changes |
| Processing overhead | Low | Very Low | REAPER: cache hits are fast<br>Standard: minimal processing needed |
| **Host Integration** |
| API access | ✅ VST2/VST3 | ✅ Standard VST | REAPER: custom extensions<br>Standard: standard VST APIs only |
| Multi-instance support | ✅ | ✅ | Per-instance storage |
| Host detection | ✅ Auto | N/A | Auto-detects REAPER environment |
| Fallback behavior | ✅ | N/A | Falls back to Standard if REAPER API unavailable |
| **Debugging** |
| Debug console | ✅ Enhanced | ✅ Basic | REAPER: shows cache stats, track info<br>Standard: basic state info |
| REAPER connection status | ✅ | N/A | Visual indicator in debug console |
| Timeline position display | ✅ | ✅ | Both show current position |

---

## 🔍 **Key Differences**

### REAPER Pipeline Advantages
1. **Timeline Access** - Can read MIDI from any point in the timeline, not limited to audio buffer
2. **Scrubbing** - Smooth scrubbing support, updates instantly when moving playhead
3. **Lookahead** - Unlimited lookahead (can read notes minutes ahead if needed)
4. **Multi-track** - Can read MIDI from different tracks than the one plugin is on
5. **Caching** - Smart caching reduces processing overhead for repeated reads
6. **Absolute Positioning** - Always knows exact PPQ position from project start

### Standard Pipeline Advantages
1. **Universal Compatibility** - Works in any VST host (Ableton, FL Studio, Logic, etc.)
2. **Simpler Architecture** - Fewer moving parts, easier to debug
3. **Lower Latency** - No cache lookup overhead (though negligible)
4. **Host Integration** - Uses standard VST APIs, no special extensions needed

---

## 🏗️ **Architecture Overview**

### REAPER Pipeline Flow
```
REAPER Project Timeline
         ↓
ReaperMidiProvider (VST2/VST3 Extensions)
         ↓
MidiCache (smart caching, invalidation)
         ↓
ReaperMidiPipeline::processCachedNotesIntoState()
         ↓
MidiInterpreter (MIDI → visual elements)
         ↓
HighwayRenderer (render to screen)
```

### Standard Pipeline Flow
```
Host MIDI Buffer
         ↓
StandardMidiPipeline::processBufferIntoState()
         ↓
MidiInterpreter (MIDI → visual elements)
         ↓
HighwayRenderer (render to screen)
```

---

## 🧪 **Testing Coverage**

| Scenario | REAPER Pipeline | Standard Pipeline |
|----------|----------------|-------------------|
| Playback from start | ✅ Tested | ✅ Tested |
| Playback mid-song | ✅ Tested | ✅ Tested |
| Scrubbing | ✅ Tested | ⚠️ Host-dependent |
| Tempo changes | ✅ Tested | ✅ Tested |
| Time signature changes | ✅ Tested | ✅ Tested |
| Multiple instances | ✅ Tested | ✅ Tested |
| Track switching (REAPER) | ✅ Tested | N/A |
| Plugin reload | ✅ Tested | ✅ Tested |

---

## 🚧 **Known Limitations**

### REAPER Pipeline
- ❌ Manual track selection required (dropdown in UI)
- ❌ Only works in REAPER (obviously)
- ⚠️ Cache invalidation on large projects can cause brief stutter
- ⚠️ Requires REAPER API availability (rare failure mode)

### Standard Pipeline
- ❌ Limited lookahead (buffer size only, typically ~512-2048 samples)
- ❌ Scrubbing support depends on host implementation
- ❌ Cannot read from other tracks
- ⚠️ May miss MIDI events if buffer is very small

---

## 🎯 **Future Improvements**

### REAPER Pipeline
- [ ] **Auto track detection** - Automatically detect which track plugin is on
- [ ] **Extended lookahead** - Use REAPER SWS extensions for even longer lookahead
- [ ] **REAPER mode toggle** - UI toggle to enable/disable REAPER mode
- [ ] **Preset integration** - Save settings to REAPER .ini files

### Standard Pipeline
- [ ] **Extended memory** - Keep notes visible when transport stops
- [ ] **Host fallback UX** - Better UI when no playhead available
- [ ] **Buffer optimization** - More efficient processing for large buffers

### Both Pipelines
- [ ] **Thread safety improvements** - Double-buffered snapshots for renderer
- [ ] **AudioProcessorValueTreeState** - Migrate parameter management
- [ ] **Audio-thread hygiene** - Remove std::function, use preallocated vectors

---

## 📝 **Implementation Notes**

### Pipeline Selection Logic
Located in `ChartPreviewAudioProcessor::prepareToPlay()`:

```cpp
// Auto-detect REAPER and initialize appropriate pipeline
if (ReaperIntegration::isReaperHost(this)) {
    pipeline = std::make_unique<ReaperMidiPipeline>(...);
} else {
    pipeline = std::make_unique<StandardMidiPipeline>(...);
}
```

### Shared Components
Both pipelines share:
- `MidiInterpreter` - MIDI → visual conversion logic
- `MidiProcessor` - Note state management
- `HighwayRenderer` - Rendering engine
- `GridlineMap` - Grid timing calculations
- `MidiUtility` - Helper functions (HOPO detection, chord detection, etc.)

### Pipeline-Specific Components
**REAPER only:**
- `ReaperMidiProvider` - API access wrapper
- `MidiCache` - Caching layer
- `ReaperVST2Extensions` / `ReaperVST3Extensions` - Host integration
- `ReaperTrackDetector` - Track enumeration

**Standard only:**
- (Uses standard JUCE `MidiBuffer` processing)

---

## 🔧 **Debugging Tips**

### REAPER Pipeline Issues
1. Check debug console for "REAPER API connected" message
2. Verify track number matches selected track
3. Check cache invalidation isn't firing too frequently
4. Ensure REAPER API is available (check VST scan logs)

### Standard Pipeline Issues
1. Verify host is sending MIDI to plugin
2. Check buffer size isn't too small (< 256 samples)
3. Ensure playhead info is available from host
4. Test with different hosts (some have quirks)

### Common Issues (Both)
1. Latency calibration - use latency slider to compensate
2. Tempo/time sig changes - verify absolute position rendering
3. Thread safety - check for race conditions in logs
4. Note accuracy - verify HOPO/chord detection settings

---

## 📚 **Related Documentation**

- [TODO.md](TODO.md) - Development roadmap
- [UPCOMING_FEATURES.md](UPCOMING_FEATURES.md) - Priority features
- [BLACK_SCREEN_FIX.md](BLACK_SCREEN_FIX.md) - v0.8.6 race condition fix
- [JUCE_REAPER_MODIFICATIONS.md](JUCE_REAPER_MODIFICATIONS.md) - JUCE framework modifications
- [development/REAPER-REFACTOR-PLAN.md](development/REAPER-REFACTOR-PLAN.md) - REAPER architecture planning
- [development/VST2-REAPER-INTEGRATION.md](development/VST2-REAPER-INTEGRATION.md) - VST2 integration details
- [development/VST3-REAPER-INTEGRATION.md](development/VST3-REAPER-INTEGRATION.md) - VST3 integration details

---

## ✅ **Version History**

### v0.8.7 - REAPER Optimization Update
- ✅ Complete REAPER pipeline implementation
- ✅ Modular pipeline architecture with factory pattern
- ✅ MIDI caching with smart invalidation
- ✅ Multi-instance support
- ✅ Time-based rendering (absolute position)
- ✅ Thread safety improvements

### v0.8.6
- ✅ Race condition fix (black screen bug)
- ✅ Debug logging optimization

### v0.8.5
- ✅ PPQ-based timing system (foundation for both pipelines)
- ✅ Latency compensation
- ✅ Gridline system

---

**Legend:**
- ✅ Fully supported / Complete
- ⚠️ Partial support / Limitations
- ❌ Not supported / Not applicable
- N/A - Not applicable to this pipeline
