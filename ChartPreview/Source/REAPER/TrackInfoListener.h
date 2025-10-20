/*
  ==============================================================================

    TrackInfoListener.h
    VST3 Standard Track Information Listener

  ==============================================================================
*/

#pragma once

#if JucePlugin_Build_VST3

#include <JuceHeader.h>
#include <pluginterfaces/vst/ivstchannelcontextinfo.h>

class ChartPreviewAudioProcessor;

class TrackInfoListener : public Steinberg::Vst::ChannelContext::IInfoListener
{
public:
    explicit TrackInfoListener(ChartPreviewAudioProcessor* proc);
    virtual ~TrackInfoListener() = default;

    // IInfoListener
    Steinberg::tresult setChannelContextInfos(Steinberg::Vst::IAttributeList* list) override;

    // IUnknown
    Steinberg::tresult queryInterface(const Steinberg::TUID _iid, void** obj) override;
    Steinberg::uint32 addRef() override;
    Steinberg::uint32 release() override;

private:
    ChartPreviewAudioProcessor* processor;
    std::atomic<Steinberg::uint32> refCount{1};
};

#endif // JucePlugin_Build_VST3
