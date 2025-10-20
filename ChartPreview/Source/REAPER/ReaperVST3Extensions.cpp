/*
  ==============================================================================

    ReaperVST3Extensions.cpp
    VST3-specific REAPER integration extensions

  ==============================================================================
*/

#include "ReaperVST3Extensions.h"
#include "TrackInfoListener.h"
#include "../PluginProcessor.h"
#include "../ReaperVST3.h"

#if JucePlugin_Build_VST3

// Define the interface IDs (once per project)
DEF_CLASS_IID(IReaperHostApplication)

ChartPreviewVST3Extensions::ChartPreviewVST3Extensions(ChartPreviewAudioProcessor* proc)
    : processor(proc), trackInfoListener(std::make_unique<TrackInfoListener>(proc))
{
}

ChartPreviewVST3Extensions::~ChartPreviewVST3Extensions() = default;

void ChartPreviewVST3Extensions::setIHostApplication(Steinberg::FUnknown* host)
{
    if (!host)
        return;

    auto reaperHost = Steinberg::FUnknownPtr<IReaperHostApplication>(host);

    if (reaperHost)
    {
        processor->isReaperHost = true;

        // Store the REAPER interface per-instance using a map
        // This allows multiple plugin instances to work simultaneously
        static std::map<ChartPreviewAudioProcessor*, Steinberg::FUnknownPtr<IReaperHostApplication>> reaperInstances;
        reaperInstances[processor] = reaperHost;

        // Create a wrapper function that looks up the correct instance
        processor->reaperGetFunc = [](const char* funcname) -> void* {
            // Find the processor in the map by iterating and trying each one
            // In practice, REAPER calls this from the audio thread of the correct instance
            for (auto& pair : reaperInstances)
            {
                if (pair.second)
                {
                    void* result = pair.second->getReaperApi(funcname);
                    if (result)
                        return result;
                }
            }
            return nullptr;
        };

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
            processor->debugText += "✅ REAPER API connected via VST3 - MIDI timeline access ready\n";
        }
        else
        {
            processor->debugText += "❌ REAPER API initialization failed after retry\n";
        }
    }
}

int32_t ChartPreviewVST3Extensions::queryIEditController(const Steinberg::TUID tuid, void** obj)
{
    // Provide TrackInfoListener for standard VST3 track info (per-instance, not static!)
    if (trackInfoListener && trackInfoListener->queryInterface(tuid, obj) == Steinberg::kResultOk)
        return Steinberg::kResultOk;

    *obj = nullptr;
    return Steinberg::kNoInterface;
}

#endif // JucePlugin_Build_VST3