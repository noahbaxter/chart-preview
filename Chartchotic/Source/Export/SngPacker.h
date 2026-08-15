#pragma once

#include <JuceHeader.h>
#include <vector>

/**
    Packs a chart into a single .sng container.

    Layout and masking follow mdsitton's reference implementation
    (github.com/mdsitton/SngFileFormat), which is the format Clone Hero reads.
    Two details in there are easy to get wrong and are worth stating up front:
    song.ini is not packed as a file, it becomes the metadata key/value
    section; and the XOR index resets at the start of every file rather than
    running across the data section.
*/
class SngPacker
{
public:
    static constexpr juce::uint32 kFormatVersion = 1;

    /** One packed file: the name it takes inside the container, and its source. */
    struct Entry
    {
        juce::String name;
        juce::File source;
    };

    struct Result
    {
        bool ok = false;
        juce::String message;
        juce::File output;
    };

    /**
        Writes `files` and `metadata` to `output`, replacing whatever was there.

        `metadata` is the song.ini contents as key/value pairs. Pairs with an
        empty key or value are dropped, matching the reference writer: the
        format has no way to say "present but blank", and a reader that finds
        one cannot tell it from a truncated file.
    */
    static Result pack(const juce::File& output,
                       const juce::StringPairArray& metadata,
                       const std::vector<Entry>& files);
};
