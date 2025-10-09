# REAPER VST2 Integration Research Report

## Project Goal
Implement REAPER timeline MIDI reading and scrubbing functionality in a JUCE VST2 plugin to bypass VST3 integration blockers.

## Current Status: ✅ Partial Success - ❌ API Access Blocked

### What Works ✅

1. **REAPER Host Detection**: 100% successful
   - Plugin correctly detects REAPER via Cockos capability queries
   - Green background visual confirmation in debug builds
   - Comprehensive logging of all REAPER-plugin communication

2. **VST2 Extension Support**: Fully implemented
   - `hasCockosNoScrollUI` - Supported
   - `hasCockosSampleAccurateAutomation` - Supported
   - `hasCockosEmbeddedUI` - Supported (disabled)
   - `wantsChannelCountNotifications` - Supported
   - `reaper_vst_extensions` - Advertised
   - `hasCockosExtensions` - Advertised with magic return value `0xbeef0000`

3. **Custom Plugin Naming**: Working
   - REAPER name queries (`index=0x2d, value=0x50`) properly handled
   - Returns custom name "Chart Preview (VST2)" with magic value `0xf00d`

4. **Debug Infrastructure**: Comprehensive
   - Real-time debug console with persistent messages
   - Detailed logging of all VST2 communication
   - Crash-safe error handling

### What Doesn't Work ❌

1. **REAPER API Handshake**: Never occurs
   - Traditional magic numbers (`0xdeadbeef`, `0xdeadf00d`, `0xdeadf00e`) never detected
   - No manufacturer-specific calls containing API function pointers
   - `effSetBypass` calls (`index=0x2a, value=2`) only contain control values (`ptr=1`, `ptr=0`)

2. **Timeline MIDI Access**: Impossible without API
   - `ReaperMidiProvider` cannot initialize without API function pointers
   - No scrubbing support during timeline navigation
   - No lookahead MIDI reading from REAPER's timeline

## Key Technical Findings

### REAPER Communication Pattern
```
1. Cockos capability queries (multiple)
2. Custom name queries (multiple instances)
3. effSetBypass control calls (ptr=1, ptr=0)
4. Repeated name queries during UI updates
```

### The Core Limitation: JUCE's audioMasterCallback
- **Traditional Method**: REAPER provides API via `audioMasterCallback(NULL, 0xdeadbeef, 0xdeadf00d, 0, "FunctionName", 0.0)`
- **JUCE Limitation**: AudioProcessor class doesn't expose audioMasterCallback to plugins
- **Forum Evidence**: "There's currently no exposure of audioMasterCallback, but it could probably be made possible with a macro" - Jules (JUCE creator)

### Known Working Solution
- **Cockos Forum**: Working JUCE VST2 REAPER API example exists
- **Requirements**: Uses JUCE "develop" branch + potentially modified JUCE files
- **Functionality**: Successfully accesses track volume, names, and timeline data

## VST2 vs VST3 Analysis

### VST2 Advantages
- Direct function pointer exchange possible
- Host-specific extensions supported
- REAPER actively queries and communicates with VST2 plugins

### VST3 Limitations
- COM interface complications
- Host-specific extensions difficult to implement
- JUCE team reluctant to add official REAPER support

### JUCE Framework Issues
- VST2: audioMasterCallback not exposed (solvable with modifications)
- VST3: Fundamental architecture barriers (major JUCE changes needed)

## Debug Output Analysis

### Successful Communication
```
✅ Supporting hasCockosNoScrollUI
✅ Supporting hasCockosSampleAccurateAutomation
✅ Provided custom name: 'Chart Preview (VST2)'
🔍 REAPER detected via Cockos capability queries!
```

### Missing Handshake
```
❌ No magic numbers detected in any manufacturer-specific calls
❌ No valid function pointers received
❌ ReaperMidiProvider remains uninitialized
```

## Implementation Quality

### Robust Error Handling
- Try/catch blocks prevent crashes from invalid function pointers
- Safe validation of all pointer values before usage
- Graceful fallbacks when API unavailable

### Comprehensive Logging
- All VST2 communication captured and displayed
- Magic number detection across all possible combinations
- Real-time debug output visible to user

### Clean Architecture
- Separated concerns: VST2Extensions, ReaperMidiProvider, MidiProcessor
- Proper resource management and initialization
- Thread-safe API access with critical sections

## Research Insights

### REAPER Integration Methods
1. **Traditional**: audioMasterCallback with magic numbers (blocked in JUCE)
2. **Modern**: Unknown alternative method (research needed)
3. **Workaround**: Modified JUCE source (confirmed working example exists)

### Alternative Approaches Investigated
- effSetBypass mechanism analysis (not the solution)
- Proactive API function discovery (impossible without callback access)
- Magic number detection across all VST2 calls (comprehensive, found nothing)

## Recommendations

### Short Term
1. **Examine Working Example**: Analyze the Cockos forum JUCE VST2 REAPER API example
2. **JUCE Modifications**: Consider modifying JUCE to expose audioMasterCallback
3. **Version Testing**: Test with different JUCE branches (develop vs stable)

### Long Term
1. **Custom VST2 Wrapper**: Bypass JUCE's VST2 wrapper entirely
2. **Direct SDK Integration**: Use Steinberg VST2 SDK with custom REAPER extensions
3. **Hybrid Approach**: JUCE for audio processing, custom wrapper for REAPER integration

## Technical Debt
- High coupling between VST2 integration and JUCE framework limitations
- Workaround solutions require ongoing maintenance of modified JUCE source
- Debug infrastructure adds runtime overhead (acceptable for development)

## Code Architecture

### Key Files
- `Source/PluginProcessor.cpp` - VST2 extensions and REAPER detection
- `Source/Midi/ReaperMidiProvider.h/.cpp` - REAPER API wrapper (ready for initialization)
- `Source/PluginEditor.cpp` - Debug console and visual feedback
- `build-vst2.sh` - Automated VST2 build script

### VST2 Extensions Implementation
```cpp
class ChartPreviewVST2Extensions : public juce::VST2ClientExtensions
{
public:
    // Handles capability queries
    juce::pointer_sized_int handleVstPluginCanDo(juce::int32 index,
                                                 juce::pointer_sized_int value,
                                                 void* ptr, float opt) override;

    // Handles manufacturer-specific calls (potential API handshake)
    juce::pointer_sized_int handleVstManufacturerSpecific(juce::int32 index,
                                                          juce::pointer_sized_int value,
                                                          void* ptr, float) override;
};
```

### REAPER MIDI Provider Interface
```cpp
class ReaperMidiProvider
{
    struct ReaperMidiNote {
        double startPPQ, endPPQ;
        int channel, pitch, velocity;
        bool selected, muted;
    };

    std::vector<ReaperMidiNote> getNotesInRange(double startPPQ, double endPPQ);
    double getCurrentPlayPosition();
    double getCurrentCursorPosition();
    bool isPlaying();
};
```

## Research Sources
- [REAPER VST Extensions Documentation](https://www.reaper.fm/sdk/vst/vst_ext.php)
- [JUCE Forum: Attaching to REAPER API VST2 vs VST3](https://forum.juce.com/t/attaching-to-the-reaper-api-vst2-vs-vst3/45459)
- [JUCE Forum: audioMasterCallback Access](https://forum.juce.com/t/is-it-possible-to-somehow-get-audiomastercallback/15141)
- [Cockos Forum: JUCE VST Plugin REAPER API Example](https://forum.cockos.com/showthread.php?t=188350)

## Conclusion
We successfully implemented comprehensive VST2 REAPER integration within JUCE's constraints. The plugin perfectly detects REAPER, handles all capability queries, and maintains robust communication. However, the core functionality (timeline MIDI access) is blocked by JUCE's architectural decision to not expose audioMasterCallback.

The path forward requires either:
1. **Modifying JUCE source** (confirmed working solution exists)
2. **Alternative integration approach** (requires further research)
3. **Different plugin framework** (abandoning JUCE for VST2)

The foundation is solid - when the API access barrier is resolved, all other components are ready for immediate REAPER timeline integration.

---

*Report generated: September 18, 2025*
*Plugin Version: VST2 Debug Build*
*JUCE Version: 7.x (standard, unmodified)*
*REAPER Version: 7.x (2024/2025)*