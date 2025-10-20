/*
  ==============================================================================

    ReaperVST3Extensions.h
    VST3-specific REAPER integration extensions

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <memory>

class ChartPreviewAudioProcessor;
class IReaperHostApplication;
class TrackInfoListener;

#if JucePlugin_Build_VST3

class ChartPreviewVST3Extensions : public juce::VST3ClientExtensions
{
public:
    ChartPreviewVST3Extensions(ChartPreviewAudioProcessor* proc);
    ~ChartPreviewVST3Extensions();

    void setIHostApplication(Steinberg::FUnknown* host) override;
    int32_t queryIEditController(const Steinberg::TUID tuid, void** obj) override;

private:
    ChartPreviewAudioProcessor* processor;
    std::unique_ptr<TrackInfoListener> trackInfoListener;
};

#endif // JucePlugin_Build_VST3