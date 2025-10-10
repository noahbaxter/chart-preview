/*
  ==============================================================================

    ReaperVST2Extensions.cpp
    VST2-specific REAPER integration extensions

  ==============================================================================
*/

#include "ReaperVST2Extensions.h"
#include "../PluginProcessor.h"

// Define static member
std::map<ChartPreviewAudioProcessor*, std::function<ChartPreviewVST2Extensions::VstHostCallbackType>*> ChartPreviewVST2Extensions::instanceCallbacks;

ChartPreviewVST2Extensions::ChartPreviewVST2Extensions(ChartPreviewAudioProcessor* proc)
    : processor(proc)
{
}

juce::pointer_sized_int ChartPreviewVST2Extensions::handleVstPluginCanDo(juce::int32 index, juce::pointer_sized_int value, void* ptr, float opt)
{
    // Check if REAPER is asking about specific capabilities
    if (ptr != nullptr)
    {
        juce::String capability = juce::String::fromUTF8((const char*)ptr);

        // Advertise that we support REAPER extensions
        if (capability == "reaper_vst_extensions")
            return 1; // Yes, we support this
        // Support Cockos VST extensions (main capability)
        else if (capability == "hasCockosExtensions")
            return 0xbeef0000; // Magic return value for Cockos extensions
        // REAPER-specific capabilities
        else if (capability == "hasCockosNoScrollUI")
            return 1; // We support this
        else if (capability == "hasCockosSampleAccurateAutomation")
            return 1; // We support this
        else if (capability == "hasCockosEmbeddedUI")
            return 0; // We don't support embedded UI
        else if (capability == "wantsChannelCountNotifications")
            return 1; // We want channel count notifications
    }

    return 0; // Return 0 for "don't know"
}

juce::pointer_sized_int ChartPreviewVST2Extensions::handleVstManufacturerSpecific(juce::int32 index, juce::pointer_sized_int value, void* ptr, float)
{
    // Handle REAPER custom plugin name query (effGetEffectName with 0x50)
    if (index == 0x2d && value == 0x50 && ptr)
    {
        // Provide a custom name for this plugin instance
        *(const char**)ptr = "Chart Preview (VST2)";
        return 0xf00d; // Magic return value indicating we handled the name query
    }

    return 0;
}

void ChartPreviewVST2Extensions::handleVstHostCallbackAvailable(std::function<VstHostCallbackType>&& callback)
{
    // Store the callback for later use
    hostCallback = std::move(callback);

    // Try to get REAPER API through direct handshake
    tryGetReaperApi();
}

void ChartPreviewVST2Extensions::tryGetReaperApi()
{
    if (!hostCallback)
        return;

    // REAPER's VST2 extension: Call audioMaster(NULL, 0xdeadbeef, 0xdeadf00d, 0, "FunctionName", 0.0)
    // to get each function directly. Test by trying to get GetPlayState.
    auto testResult = hostCallback(0xdeadbeef, 0xdeadf00d, 0, (void*)"GetPlayState", 0.0);

    if (testResult != 0)
    {
        processor->isReaperHost = true;

        // Store callback in per-instance map so our wrapper function can access it
        instanceCallbacks[processor] = &hostCallback;

        // Create a wrapper function that looks up the correct callback for this processor
        // The wrapper is stored as a stateless lambda that can be converted to a function pointer
        // We store the processor pointer in the map and look it up when needed
        auto wrapperFunc = +[](const char* funcname) -> void* {
            // Try each instance's callback until we find one that works
            // In most cases, this will be the first one tried
            for (auto& pair : ChartPreviewVST2Extensions::instanceCallbacks)
            {
                if (pair.second)
                {
                    auto& callback = *pair.second;
                    // Test if this callback works by calling it
                    auto result = callback(0xdeadbeef, 0xdeadf00d, 0, (void*)funcname, 0.0);
                    if (result != 0)
                        return (void*)result;
                }
            }
            return nullptr;
        };

        processor->reaperGetFunc = wrapperFunc;

        // Initialize the REAPER MIDI provider with retry logic
        bool initialized = processor->reaperMidiProvider.initialize(processor->reaperGetFunc);

        // Retry once if initialization failed (handles race conditions)
        if (!initialized)
        {
            processor->debugText += "⚠️  REAPER API initialization failed, retrying...\n";
            juce::Thread::sleep(50); // Brief delay
            initialized = processor->reaperMidiProvider.initialize(processor->reaperGetFunc);
        }

        if (initialized)
        {
            processor->reaperMidiProvider.setLogger(&processor->getDebugLogger());
            processor->debugText += "✅ REAPER API connected - MIDI timeline access ready\n";
        }
        else
        {
            processor->debugText += "❌ REAPER API initialization failed after retry\n";
        }
    }
}