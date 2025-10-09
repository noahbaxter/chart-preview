# JUCE Modifications for REAPER Integration

## Overview

This document explains the **required JUCE modifications** for accessing REAPER's API from VST plugins. Unlike what some documentation suggests, **you MUST modify JUCE** to get clean REAPER integration working.

## The Problem

REAPER provides a powerful C++ API for plugins to access timeline data, MIDI notes, track information, and more. However, accessing this API from JUCE-based plugins requires modifying JUCE's source code because:

1. **VST2**: Stock JUCE provides `handleVstHostCallbackAvailable()` but doesn't perform the REAPER handshake automatically
2. **VST3**: Stock JUCE doesn't expose the VST3 host context in a way that allows querying for REAPER's `IReaperHostApplication` interface

## What Everyone Else Does

Looking at the JUCE/REAPER development community:

- **Xenakios** (2016-2018): Maintained a fork of JUCE with REAPER modifications
- **GavinRay97** (2021): Created patches for VST3 REAPER UI embedding ([JUCE-reaper-embedded-fx-gui](https://github.com/GavinRay97/JUCE-reaper-embedded-fx-gui))
- **This Project** (2025): Custom modifications for VST2 REAPER API access

**Every developer who wants full REAPER integration ends up modifying JUCE.**

## Official JUCE Support Status

### What Stock JUCE Provides

#### VST2
- `VST2ClientExtensions::handleVstHostCallbackAvailable()` - Gives you the raw `audioMaster` callback
- `VST2ClientExtensions::handleVstPluginCanDo()` - Can advertise REAPER capabilities
- `VST2ClientExtensions::handleVstManufacturerSpecific()` - Can handle REAPER messages

**However**: You must manually perform the REAPER handshake yourself in your plugin code.

#### VST3
- `VST3ClientExtensions::setIHostApplication()` - Receives `FUnknown*` host pointer
- `VST3ClientExtensions::queryIEditController()` - Can provide custom interfaces

**However**: The `FUnknown*` is only forward-declared; you cannot easily call `queryInterface()` on it without linking against the full VST3 SDK.

### Official Example: ReaperEmbeddedViewPluginDemo

JUCE includes `examples/Plugins/ReaperEmbeddedViewPluginDemo.h` (added around June 2021) that demonstrates:
- Embedding a custom UI in REAPER's track control panel
- Using both VST2 and VST3 extension points
- Accessing REAPER's global bypass function

**Important**: This example focuses on **UI embedding**, not **API access for MIDI/timeline data**. It shows the extension points exist, but doesn't demonstrate accessing REAPER's main API functions.

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

### Why This Is Better Than Stock JUCE

**Stock JUCE Approach** (what you'd have to do):
```cpp
void handleVstHostCallbackAvailable(std::function<VstHostCallbackType>&& callback) override
{
    hostCallback = std::move(callback);

    // You have to do the handshake yourself:
    auto testResult = hostCallback(0xdeadbeef, 0xdeadf00d, 0, (void*)"GetPlayState", 0.0);
    if (testResult != 0)
    {
        // Now create a wrapper function...
        // And store static pointers...
        // And manage the callback lifetime...
    }
}
```

**Our Approach**:
```cpp
void handleReaperApi(void* (*reaperGetFunc)(const char*)) override
{
    // Just use it!
    auto GetPlayState = (int(*)())reaperGetFunc("GetPlayState");
}
```

## VST3 Support (TODO)

### Current Status
❌ Not yet implemented

### What Needs to Happen

Looking at GavinRay97's patches as reference:

#### 1. Patch `juce_AudioProcessor.h`
Add a new virtual method that receives the VST3 host context:

```cpp
#ifdef JUCE_VST3_ENABLE_PASS_HOST_CONTEXT_TO_AUDIO_PROCESSOR_ON_INITIALIZE
virtual void handleVST3HostContext(
    Steinberg::FUnknown* hostContext,
    Steinberg::Vst::IHostApplication* host,
    JuceAudioProcessor* comPluginInstance,
    JuceVST3EditController* juceVST3EditController) {};
#endif
```

#### 2. Patch `juce_VST3_Wrapper.cpp`
Call the method during initialization:

```cpp
#ifdef JUCE_VST3_ENABLE_PASS_HOST_CONTEXT_TO_AUDIO_PROCESSOR_ON_INITIALIZE
    getPluginInstance().handleVST3HostContext(
        hostContext,
        this->host.get(),
        this->comPluginInstance.get(),
        this->juceVST3EditController.get()
    );
#endif
```

#### 3. Use It in Your Plugin

```cpp
void handleVST3HostContext(Steinberg::FUnknown* hostContext, ...) override
{
    // Now you can query for IReaperHostApplication
    void* objPtr = nullptr;
    if (hostContext->queryInterface(reaper::IReaperHostApplication::iid, &objPtr) == Steinberg::kResultOk)
    {
        auto* reaperHost = static_cast<reaper::IReaperHostApplication*>(objPtr);
        auto MIDI_GetNote = reaperHost->getReaperApi("MIDI_GetNote");
        // ...
    }
}
```

### Alternative: Use VST3ClientExtensions (More Complex)

Stock JUCE's `VST3ClientExtensions::setIHostApplication()` provides the host pointer, but you need to:
1. Manually handle the `queryInterface()` call (requires VST3 SDK knowledge)
2. Deal with `FUnknown` being forward-declared only
3. Manage COM reference counting

See `examples/Plugins/ReaperEmbeddedViewPluginDemo.h` for an example, though it uses REAPER's header files that help with this.

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

**The Reality**: Despite JUCE's official `ReaperEmbeddedViewPluginDemo` example, **you need to modify JUCE** for practical REAPER integration because:

1. **VST2**: The official extension points require you to manually do the handshake and manage callbacks
2. **VST3**: The host context isn't exposed in a usable way for querying custom interfaces

Our modifications make this cleaner by:
- Automatically performing the REAPER handshake in VST2
- Providing a simple callback with the function pointer ready to use
- Eliminating boilerplate code from your plugin

This is the same conclusion reached by Xenakios, GavinRay97, and other JUCE developers who've tackled REAPER integration.
