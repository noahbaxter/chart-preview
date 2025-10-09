# JUCE Modifications for REAPER Integration

## Overview

Stock JUCE cannot access REAPER's API. This document explains required modifications for MIDI/timeline access from VST plugins.

## The Problem

REAPER provides a C++ API for accessing timeline data, MIDI notes, track information, etc. Accessing this from JUCE plugins requires source modifications:

1. **VST2**: Stock JUCE provides `handleVstHostCallbackAvailable()` but doesn't perform REAPER handshake automatically
2. **VST3**: Stock JUCE doesn't expose VST3 host context for querying `IReaperHostApplication` interface

## Community Precedent

Every developer who wants full REAPER integration modifies JUCE:

- **Xenakios** (2016-2018): Maintained JUCE fork with REAPER modifications
- **GavinRay97** (2021): Created patches for VST3 REAPER UI embedding ([JUCE-reaper-embedded-fx-gui](https://github.com/GavinRay97/JUCE-reaper-embedded-fx-gui))
- **This Project** (2025): Custom modifications for VST2 REAPER API access

## Stock JUCE Limitations

**VST2**: Extension points exist but require manual handshake and callback management in plugin code.

**VST3**: `setIHostApplication()` provides `FUnknown*` pointer, but it's forward-declared only; cannot call `queryInterface()` without full VST3 SDK.

**Official Example**: JUCE includes `ReaperEmbeddedViewPluginDemo.h` for UI embedding, but not MIDI/timeline API access.

## Our Modifications for VST2

### Modification Location
`third_party/JUCE/modules/` (local JUCE copy)

### Git Commit
Branch: `reaper-vst2-extensions`
Commit: `3eaddc19c5` - "Expose audioMasterCallback to VST2 plugins for REAPER API access"

### What Changed

#### 1. Added New Method to `VST2ClientExtensions`
**File**: `modules/juce_audio_processors/utilities/juce_VST2ClientExtensions.h`

```cpp
/** This is called if the host is REAPER and provides API functions.
    The function pointer can be used to get REAPER API functions by name.
    @param reaperGetFunc A function pointer to get REAPER API functions
*/
virtual void handleReaperApi (void* (*reaperGetFunc)(const char*)) {}
```

#### 2. Modified VST2 Wrapper to Perform REAPER Handshake
**File**: `modules/juce_audio_plugin_client/juce_audio_plugin_client_VST2.cpp`

```cpp
// Try to get REAPER API functions if we're in REAPER
// This attempts the standard REAPER handshake using the magic numbers
auto reaperGetFunc = (void*(*)(const char*))audioMaster(nullptr, 0xdeadbeef, 0xdeadf00d, 0, nullptr, 0.0);
if (reaperGetFunc)
{
    // We have the REAPER API! Now pass it to the plugin if it wants it
    if (auto* callbackHandler = processorPtr->getVST2ClientExtensions())
    {
        callbackHandler->handleReaperApi(reaperGetFunc);
    }
}
```

### How to Use It

In your `AudioProcessor` subclass:

```cpp
class MyVST2Extensions : public VST2ClientExtensions
{
public:
    void handleReaperApi(void* (*reaperGetFunc)(const char*)) override
    {
        // Store the function pointer
        this->reaperGetFunc = reaperGetFunc;

        // Now you can get any REAPER API function:
        auto GetPlayState = (int(*)())reaperGetFunc("GetPlayState");
        auto MIDI_GetNote = (bool(*)(MediaItem_Take*, int, ...))reaperGetFunc("MIDI_GetNote");

        // Initialize your REAPER integration
        reaperMidiProvider.initialize(reaperGetFunc);
    }

private:
    void* (*reaperGetFunc)(const char*) = nullptr;
};

VST2ClientExtensions* getVST2ClientExtensions() override
{
    if (!vst2Extensions)
        vst2Extensions = std::make_unique<MyVST2Extensions>();
    return vst2Extensions.get();
}
```

### Benefits Over Stock JUCE

Our modifications eliminate boilerplate: handshake is automatic, function pointer is ready to use immediately.

## VST3 Support (TODO)

❌ Not yet implemented. See GavinRay97's patches for reference approach: add `handleVST3HostContext()` virtual method to pass host context, then query for `IReaperHostApplication` interface.

Alternative: Use stock `VST3ClientExtensions::setIHostApplication()` but requires manual `queryInterface()` handling and COM reference counting.

## The REAPER VST2 API Handshake

### Magic Numbers
- `0xdeadbeef` - First magic number (opcode)
- `0xdeadf00d` - Second magic number (index)

### How It Works

```cpp
// REAPER checks if your plugin wants the API
void* result = audioMaster(nullptr, 0xdeadbeef, 0xdeadf00d, 0, nullptr, 0.0);

if (result != 0)
{
    // result is a function pointer: void* (*)(const char*)
    auto reaperGetFunc = (void*(*)(const char*))result;

    // Now you can get any REAPER function by name:
    auto GetPlayState = (int(*)())reaperGetFunc("GetPlayState");
}
```

### Available REAPER Functions

From `reaper_plugin.h` (REAPER SDK), you can access hundreds of functions including:

**Timeline Access**:
- `GetPlayPosition()`, `GetPlayPosition2Ex()`
- `GetCursorPosition()`, `GetCursorPosition2Ex()`
- `GetPlayState()` - Returns play/pause/record state

**MIDI Access**:
- `MIDI_CountEvts()` - Count MIDI events in a take
- `MIDI_GetNote()` - Get specific MIDI note data
- `MIDI_EnumSelNotes()` - Enumerate selected notes

**Project/Track Access**:
- `GetTrack()` - Get track by index
- `GetActiveTake()` - Get active MIDI take
- `GetMediaItem()` - Get media items
- `CountTracks()` - Get track count

**And Many More**: See `third_party/reaper-sdk/sdk/reaper_plugin.h` for the full list (1000+ functions).

## Implementation in This Project

### Files

**VST2 Extensions Implementation**:
- `Source/PluginProcessor.h:38-54` - `ChartPreviewVST2Extensions` class declaration
- `Source/PluginProcessor.cpp:218-325` - VST2 extensions implementation

**REAPER API Wrapper**:
- `Source/Midi/ReaperMidiProvider.h` - Interface for accessing REAPER MIDI data
- `Source/Midi/ReaperMidiProvider.cpp` - Implementation using REAPER API functions

**Integration Points**:
- `Source/PluginProcessor.cpp:264` - `handleVstHostCallbackAvailable()` receives callback
- `Source/PluginProcessor.cpp:273` - `tryGetReaperApi()` performs handshake
- `Source/PluginProcessor.cpp:302` - Initializes `ReaperMidiProvider`

### Key Code Sections

#### Detecting REAPER and Getting the API
`Source/PluginProcessor.cpp:273-305`:
```cpp
void tryGetReaperApi()
{
    if (!hostCallback)
        return;

    // REAPER's VST2 extension: Call audioMaster(NULL, 0xdeadbeef, 0xdeadf00d, ...)
    auto testResult = hostCallback(0xdeadbeef, 0xdeadf00d, 0, (void*)"GetPlayState", 0.0);

    if (testResult != 0)
    {
        processor->isReaperHost = true;

        // Create wrapper function for the API
        static auto reaperApiWrapper = [](const char* funcname) -> void* {
            // ... returns REAPER functions by name
        };

        processor->reaperGetFunc = reaperApiWrapper;
        processor->reaperMidiProvider.initialize(processor->reaperGetFunc);
    }
}
```

#### Using the REAPER API
`Source/Midi/ReaperMidiProvider.cpp`:
```cpp
void ReaperMidiProvider::initialize(void* (*getFunc)(const char*))
{
    reaperGetFunc = getFunc;

    // Get all the REAPER functions we need
    MIDI_CountEvts = (int(*)(void*, int*, int*, int*))reaperGetFunc("MIDI_CountEvts");
    MIDI_GetNote = (bool(*)(void*, int, bool*, bool*, double*, double*, int*, int*, int*))
        reaperGetFunc("MIDI_GetNote");
    GetPlayPosition = (double(*)())reaperGetFunc("GetPlayPosition");
    // ... etc
}
```

## Testing Your REAPER Integration

### Visual Indicators
`Source/PluginEditor.cpp:132-165` provides colored backgrounds:
- **Green**: REAPER connected successfully
- **Red**: REAPER detected but connection failed
- **Blue**: No REAPER provider initialized
- **Yellow**: Buffer mode (non-REAPER build)

### Debug Output
`Source/PluginProcessor.cpp:303`:
```cpp
processor->debugText += "✅ REAPER API connected - MIDI timeline access ready\n";
```

Check your plugin's debug output for connection status.

## Resources

### REAPER SDK
- `third_party/reaper-sdk/sdk/reaper_plugin.h` - Full API function declarations
- `third_party/reaper-sdk/sdk/reaper_vst3_interfaces.h` - VST3 interface definitions

### JUCE Examples
- `/Applications/JUCE/examples/Plugins/ReaperEmbeddedViewPluginDemo.h` - Official JUCE example (UI embedding)

### External Projects
- [JUCE-reaper-embedded-fx-gui](https://github.com/GavinRay97/JUCE-reaper-embedded-fx-gui) - GavinRay97's VST3 patches and example
- [Xenakios's Bitbucket](https://bitbucket.org/xenakios/reaper_api_vst_example) - Historical VST2 example (may be down)

### Forum Discussions
- [JUCE Forum: Attaching to REAPER API VST2 vs VST3](https://forum.juce.com/t/attaching-to-the-reaper-api-vst2-vs-vst3/45459)
- [Cockos Forum: JUCE plugin using REAPER VST extensions](https://forums.cockos.com/showthread.php?t=253505)
- [JUCE GitHub Issue #902](https://github.com/juce-framework/JUCE/issues/902) - Custom VST3 interface registration discussion

## Summary

Stock JUCE requires manual REAPER integration. Our modifications automate the VST2 handshake and eliminate boilerplate, following the same approach as Xenakios, GavinRay97, and other JUCE developers who've tackled REAPER integration.
