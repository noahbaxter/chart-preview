/*
  ==============================================================================

    TrackInfoListener.cpp
    VST3 Standard Track Information Listener

  ==============================================================================
*/

#include "TrackInfoListener.h"
#include "../PluginProcessor.h"

// The shared library is built with both VST3=1 and AU=1. The VST3 code references
// IInfoListener::iid, so the symbol must be available.
// - For AU builds, we must define it here, as the VST3 SDK's vstinitiids.cpp is not included.
// - For VST3 builds, vstinitiids.cpp provides the definition.
//
// To solve this, we define the IID only for AU builds, and mark it as a weak symbol.
// This prevents a "duplicate symbol" linker error in VST3 builds.
#if JucePlugin_Build_AU
using namespace Steinberg;
__attribute__((weak))
DEF_CLASS_IID(Vst::ChannelContext::IInfoListener)
#endif

#if JucePlugin_Build_VST3

TrackInfoListener::TrackInfoListener(ChartPreviewAudioProcessor* proc)
    : processor(proc)
{
}

tresult TrackInfoListener::setChannelContextInfos(Vst::IAttributeList* list)
{
    if (!list || !processor)
        return kInvalidArgument;

    // Try to get channel index (1-based)
    int64 channelIndex = -1;
    if (list->getInt(Vst::ChannelContext::kChannelIndexKey, channelIndex) == kResultTrue)
    {
        int trackNum = static_cast<int>(channelIndex);

        // Only update if track number actually changed
        if (processor->detectedTrackNumber.load() != trackNum)
        {
            processor->detectedTrackNumber = trackNum;

            // Auto-apply track change (convert from 1-based to 0-based for MIDI channel)
            processor->applyTrackNumberChange(trackNum - 1);
        }
    }

    return kResultTrue;
}

tresult TrackInfoListener::queryInterface(const TUID _iid, void** obj)
{
    if (std::memcmp(_iid, Vst::ChannelContext::IInfoListener::iid, sizeof(TUID)) == 0)
    {
        ++refCount;
        *obj = this;
        return kResultOk;
    }

    *obj = nullptr;
    return kNoInterface;
}

uint32 TrackInfoListener::addRef()
{
    return ++refCount;
}

uint32 TrackInfoListener::release()
{
    uint32 newCount = --refCount;
    if (newCount == 0)
        delete this;
    return newCount;
}

#endif // JucePlugin_Build_VST3
