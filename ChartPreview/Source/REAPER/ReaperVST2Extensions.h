/*
  ==============================================================================

    ReaperVST2Extensions.h
    VST2-specific REAPER integration extensions

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class ChartPreviewAudioProcessor;

class ChartPreviewVST2Extensions : public juce::VST2ClientExtensions
{
public:
    ChartPreviewVST2Extensions(ChartPreviewAudioProcessor* proc);

    juce::pointer_sized_int handleVstPluginCanDo(juce::int32 index, juce::pointer_sized_int value, void* ptr, float opt) override;
    juce::pointer_sized_int handleVstManufacturerSpecific(juce::int32 index, juce::pointer_sized_int value, void* ptr, float) override;
    void handleVstHostCallbackAvailable(std::function<VstHostCallbackType>&& callback) override;

private:
    void tryGetReaperApi();

    ChartPreviewAudioProcessor* processor;
    std::function<VstHostCallbackType> hostCallback;

    // Per-instance storage for callbacks (keyed by processor pointer)
    static std::map<ChartPreviewAudioProcessor*, std::function<VstHostCallbackType>*> instanceCallbacks;
};