#pragma once

#include <JuceHeader.h>

/**
    Makes a chart background out of the album art.

    Album art is square and small; a background is wide and sits behind the
    highway. Scaling the cover up to fill the frame is the whole trick, with
    enough blur and darkening that it reads as a backdrop rather than as a
    stretched, distracting cover.
*/
class BackgroundGenerator
{
public:
    /** 16:9 at a size that still looks right on a 4K display without being huge. */
    static constexpr int kWidth = 1920;
    static constexpr int kHeight = 1080;

    struct Options
    {
        /** Blur radius as a fraction of the frame height. */
        float blur = 0.018f;
        /** How far towards black the result is pulled. */
        float darken = 0.25f;
    };

    /** Empty image when the source cannot be read. */
    static juce::Image generate(const juce::File& artwork, const Options& options);

    /** Writes the generated background beside `artwork`'s destination. */
    static bool writeTo(const juce::File& target, const juce::File& artwork,
                        const Options& options);
};
