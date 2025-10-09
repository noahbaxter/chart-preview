# REAPER Integration - Historical Context

**Why This Exists**: Documents the journey from VST3 blockers to working VST2 solution

---

## Original Goal

Enable ChartPreview to read MIDI notes directly from REAPER's timeline for:
1. **Scrubbing** - Instant visual feedback when dragging cursor while paused
2. **Lookahead** - Show notes ahead without VST buffer/latency constraints

**Problem**: VST audio buffers only provide MIDI in real-time playback. We need timeline access for scrubbing and lookahead.

---

## Why VST3 Didn't Work

### The Blocker
REAPER exposes timeline API via `IReaperHostApplication` interface. To access it:
```cpp
// Need to query VST3 host for REAPER interface
Steinberg::FUnknownPtr<IReaperHostApplication> reaperHost(hostContext);
auto MIDI_GetNote = reaperHost->getReaperApi("MIDI_GetNote");
```

### JUCE's Limitation
- JUCE provides `VST3ClientExtensions::setIHostApplication(FUnknown* host)`
- But `FUnknown` is only forward-declared (no implementation)
- Cannot call `queryInterface()` without full VST3 SDK
- Linking against full VST3 SDK conflicts with JUCE's wrapper

### What We Tried
1. Manual vtable calls → Crashed (wrong vtable layout)
2. Using FUID class → Linker errors
3. FUnknownPtr template → Requires proper FUnknown inheritance

**Conclusion**: VST3 requires major JUCE modifications or bypassing JUCE's wrapper entirely.

---

## Why VST2 Works

### The Solution
VST2 uses simple function pointer exchange instead of COM interfaces:

```cpp
// REAPER's audioMaster callback with magic numbers
auto reaperFunc = audioMaster(NULL, 0xdeadbeef, 0xdeadf00d, 0, "FunctionName", 0.0);
```

### Advantages Over VST3
- ✅ No COM interface complexity
- ✅ No linking issues (pure function pointers)
- ✅ JUCE exposes `handleVstHostCallbackAvailable()` (with modifications)
- ✅ Well-documented pattern (SWS extensions, forum examples)

### Trade-offs
- ❌ VST2 deprecated by Steinberg (but REAPER still supports it)
- ❌ Some modern DAWs dropping VST2
- ✅ **But**: For REAPER-specific features, VST2 is pragmatic choice

---

## The Solution Path

**Decision**: Use VST2 with JUCE modifications

**What We Had to Modify**:
1. Add `handleReaperApi()` callback to `VST2ClientExtensions`
2. Modify VST2 wrapper to perform REAPER handshake automatically
3. Pass function pointer to plugin

**Result**: Working REAPER API access with minimal JUCE changes

See `/docs/JUCE_REAPER_MODIFICATIONS.md` for exact changes
See `/docs/development/VST2-REAPER-INTEGRATION.md` for implementation

---

## Key Learnings

1. **Stock JUCE Cannot Access REAPER API** (VST2 or VST3)
   - Everyone modifies JUCE for REAPER integration
   - No official JUCE support planned

2. **VST2 Is Simpler for Host-Specific Extensions**
   - Function pointers > COM interfaces
   - Less framework dependency

3. **REAPER Uses 960 PPQ Resolution**
   - Critical discovery for timeline sync
   - Must convert REAPER PPQ to VST playhead scale

4. **Community Precedent**
   - Xenakios (2016-2018): Modified JUCE fork
   - GavinRay97 (2021): VST3 UI embedding patches
   - This project (2025): VST2 timeline access modifications

---

## Timeline

- **Phase 1**: Attempted VST3 integration → Blocked by JUCE/COM issues
- **Phase 2**: Researched VST2 alternative → Found simpler pattern
- **Phase 3**: Modified JUCE for VST2 → ✅ Working solution
- **Current**: VST2 API access operational, timeline MIDI reading works

---

## References

**Forum Discussions**:
- [JUCE: Attaching to REAPER API VST2 vs VST3](https://forum.juce.com/t/attaching-to-the-reaper-api-vst2-vs-vst3/45459)
- [Cockos: JUCE VST plugin REAPER API example](https://forum.cockos.com/showthread.php?t=188350)

**Reference Projects**:
- `third_party/reference/JUCE-reaper-embedded-fx-gui/` - VST3 UI embedding (different use case)
- SWS Extension - VST2 REAPER API patterns

**REAPER SDK**:
- `third_party/reaper-sdk/sdk/reaper_plugin.h` - API function declarations
- `third_party/reaper-sdk/sdk/reaper_vst3_interfaces.h` - VST3 interface (couldn't use)
