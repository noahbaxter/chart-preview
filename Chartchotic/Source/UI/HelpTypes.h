#pragma once

#include <JuceHeader.h>
#include <vector>

struct HelpSegment
{
    juce::String text;
    bool isKey = false;
};

using HelpText = std::vector<HelpSegment>;

inline HelpText makeHelp(std::initializer_list<HelpSegment> segs) { return segs; }
inline HelpSegment key(const juce::String& t) { return { t, true }; }
inline HelpSegment dim(const juce::String& t) { return { t, false }; }
