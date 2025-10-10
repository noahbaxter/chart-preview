/*
  ==============================================================================

    ReaperVST3Extensions.cpp
    VST3-specific REAPER integration extensions

  ==============================================================================
*/

#include "ReaperVST3Extensions.h"
#include "../PluginProcessor.h"
#include "../ReaperVST3.h"

#if JucePlugin_Build_VST3

// Define the interface IDs (once per project)
DEF_CLASS_IID(IReaperHostApplication)

ChartPreviewVST3Extensions::ChartPreviewVST3Extensions(ChartPreviewAudioProcessor* proc)
    : processor(proc)
{
}

void ChartPreviewVST3Extensions::setIHostApplication(Steinberg::FUnknown* host)
{
    if (!host)
        return;

    // Use FUnknownPtr for automatic COM handling (from reference project)
    auto reaper = FUnknownPtr<IReaperHostApplication>(host);
    if (reaper)
    {
        processor->isReaperHost = true;

        // Create a wrapper function to call getReaperApi
        // Store reaper interface in static variable for the function pointer
        static FUnknownPtr<IReaperHostApplication> staticReaper = reaper;
        static auto reaperApiWrapper = [](const char* funcname) -> void* {
            if (!staticReaper)
                return nullptr;
            return staticReaper->getReaperApi(funcname);
        };

        processor->reaperGetFunc = reaperApiWrapper;

        // Initialize the REAPER MIDI provider
        processor->reaperMidiProvider.initialize(processor->reaperGetFunc);
        processor->debugText += "✅ REAPER API connected via VST3 - MIDI timeline access ready\n";
    }
}

#endif // JucePlugin_Build_VST3