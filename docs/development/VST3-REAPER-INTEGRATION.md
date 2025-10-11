# VST3 REAPER Integration - Implementation Guide

**Status**: ✅ Implemented and Building Successfully
**Date**: October 9, 2025

## Overview

VST3 REAPER integration provides the same timeline MIDI access as VST2, using REAPER's `IReaperHostApplication` interface. This allows the plugin to read MIDI notes directly from the project timeline, enabling scrubbing support and lookahead beyond the audio buffer.

## Implementation

### Files Modified

1. **`Source/PluginProcessor.h`** - Added `getVST3ClientExtensions()` declaration
2. **`Source/PluginProcessor.cpp`** - Added `ChartPreviewVST3Extensions` class and implementation
3. **`Source/ReaperVST3.h`** (new) - REAPER VST3 interface wrapper
4. **`ChartPreview.jucer`** - Added ReaperVST3.h to project

### Key Implementation Details

Based on GavinRay97's reference project ([JUCE-reaper-embedded-fx-gui](https://github.com/GavinRay97/JUCE-reaper-embedded-fx-gui)), the implementation uses a simple approach:

```cpp
// In ReaperVST3.h
using namespace Steinberg;
#include "reaper_vst3_interfaces.h"
DEF_CLASS_IID(IReaperHostApplication)

// In PluginProcessor.cpp - VST3 extensions class
class ChartPreviewVST3Extensions : public juce::VST3ClientExtensions
{
public:
    void setIHostApplication(Steinberg::FUnknown* host) override
    {
        auto reaper = FUnknownPtr<IReaperHostApplication>(host);
        if (reaper)
        {
            // Store interface and create function pointer wrapper
            static FUnknownPtr<IReaperHostApplication> staticReaper = reaper;
            static auto reaperApiWrapper = [](const char* funcname) -> void* {
                return staticReaper ? staticReaper->getReaperApi(funcname) : nullptr;
            };

            processor->reaperGetFunc = reaperApiWrapper;
            processor->reaperMidiProvider.initialize(processor->reaperGetFunc);
        }
    }
};
```

### How It Works

1. **JUCE calls `setIHostApplication()`** when the VST3 host provides its `IHostApplication` interface
2. **Query for REAPER interface** using `FUnknownPtr<IReaperHostApplication>` (automatic COM handling)
3. **Create function pointer wrapper** using a lambda that captures the REAPER interface
4. **Initialize `ReaperMidiProvider`** with the function pointer - same as VST2!

### Key Differences from VST2

| Aspect | VST2 | VST3 |
|--------|------|------|
| **Entry point** | `handleVstHostCallbackAvailable()` | `setIHostApplication()` |
| **API access** | Magic numbers `0xdeadbeef/0xdeadf00d` | Query `IReaperHostApplication` interface |
| **Interface handling** | Manual callback management | `FUnknownPtr<>` for automatic COM |
| **Function access** | Direct audioMaster callback | `getReaperApi()` method |

### Shared Components

Both VST2 and VST3 use the same:
- `ReaperMidiProvider` class for timeline access
- `reaperGetFunc` function pointer signature: `void* (*)(const char*)`
- REAPER API functions: `MIDI_GetNote()`, `GetPlayPosition()`, etc.

## Build Configuration

### Requirements

- JUCE with VST3 SDK included (v7.0.12+)
- REAPER SDK headers in `third_party/reaper-sdk/sdk/`
- VST3 format enabled in `.jucer` file

### Build Commands

**Local Testing** (builds VST2+VST3):
```bash
cd ChartPreview
./build-scripts/build-local-test.sh              # Build only
./build-scripts/build-local-test.sh --open-reaper  # Build + launch REAPER
```

**Release Builds**:
- **macOS**: `./build-scripts/build-macos-release.sh`
- **Windows**: Build VST3 target in Visual Studio 2022
- **Linux**: `make CONFIG=Release` in `Builds/LinuxMakefile/`

## Testing

### Verification Steps

1. Build VST3 plugin
2. Load in REAPER
3. Check for **green background** (indicates REAPER connection)
4. Verify debug output: "✅ REAPER API connected via VST3"
5. Test timeline scrubbing and note display

### Expected Behavior

- Plugin connects to REAPER automatically on load
- Timeline MIDI notes are accessible via `ReaperMidiProvider`
- Same functionality as VST2 version
- No COM interface leaks (handled by `FUnknownPtr`)

## Troubleshooting

**Build Errors - "FUnknown not in namespace":**
- Ensure `using namespace Steinberg;` before including `reaper_vst3_interfaces.h`
- Include VST3 SDK headers: `<pluginterfaces/base/funknown.h>`

**No REAPER Connection:**
- Make sure you loaded the **VST3** version (not VST2 or AU)
- Check that `isReaperHost` flag is set
- Verify `IReaperHostApplication::iid` is properly defined with `DEF_CLASS_IID`

**Crash on Unload:**
- `FUnknownPtr` handles reference counting automatically
- Don't manually call `release()` on the interface

## References

### Implementation

- **GavinRay97's Reference**: [JUCE-reaper-embedded-fx-gui](https://github.com/GavinRay97/JUCE-reaper-embedded-fx-gui)
- **REAPER VST Extensions**: `third_party/reaper-sdk/docs/VST_EXTENSIONS.md`
- **REAPER SDK**: `third_party/reaper-sdk/sdk/reaper_vst3_interfaces.h`

### JUCE Integration Points

- `VST3ClientExtensions::setIHostApplication()` - PluginProcessor.h:84
- VST3 wrapper queries extensions: `juce_audio_plugin_client_VST3.cpp:1527`
- Interface definition: `reaper_vst3_interfaces.h:4`

## Next Steps

With both VST2 and VST3 REAPER integration complete:

1. ✅ VST2 integration (completed)
2. ✅ VST3 integration (completed)
3. 🚧 Display logic for timeline-based lookahead (TODO)
4. 🚧 Scrubbing support in UI (TODO)
5. 🚧 Integration with visual rendering window (TODO)

See `docs/development/VST2-REAPER-INTEGRATION.md` for VST2 implementation details.
