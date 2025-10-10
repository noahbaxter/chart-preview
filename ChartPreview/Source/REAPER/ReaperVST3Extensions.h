/*
  ==============================================================================

    ReaperVST3Extensions.h
    VST3-specific REAPER integration extensions

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class ChartPreviewAudioProcessor;

#if JucePlugin_Build_VST3

class ChartPreviewVST3Extensions : public juce::VST3ClientExtensions
{
public:
    ChartPreviewVST3Extensions(ChartPreviewAudioProcessor* proc);

    void setIHostApplication(Steinberg::FUnknown* host) override;

private:
    ChartPreviewAudioProcessor* processor;
};

#endif // JucePlugin_Build_VST3