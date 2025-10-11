#pragma once

// REAPER VST3 interface wrapper
// Based on GavinRay97's approach: https://github.com/GavinRay97/JUCE-reaper-embedded-fx-gui
#if JucePlugin_Build_VST3

#include <pluginterfaces/base/funknown.h>
#include <pluginterfaces/base/ftypes.h>

// Use Steinberg namespace to simplify reaper_vst3_interfaces.h inclusion
using namespace Steinberg;

// Now include REAPER VST3 interfaces with proper namespace context
#include "../../../third_party/reaper-sdk/sdk/reaper_vst3_interfaces.h"

// The interface ID definition is moved to ReaperVST3Extensions.cpp
// to avoid duplicate symbol errors

#endif // JucePlugin_Build_VST3
