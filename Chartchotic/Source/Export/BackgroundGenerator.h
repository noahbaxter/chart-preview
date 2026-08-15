#pragma once

#include <JuceHeader.h>

/**
    Makes a chart background out of the album art.

    Album art is square and small; a background is wide and sits behind the
    highway. Scaling the cover up to fill the frame is the shared trick, and
    the styles differ in what they put back on top of it.

    Ported from click-midi's folder_gen.py, which generated the backgrounds
    already shipping, so charts made either way come out the same.
*/
class BackgroundGenerator
{
public:
    /** What folder_gen.py caps at, and what the shipped backgrounds are. */
    static constexpr int kMaxWidth = 2560;
    static constexpr int kMaxHeight = 1440;

    enum class Style
    {
        /** Blurred fill with the cover on top of it, raised above centre. */
        coverOnBlur,
        /** Blurred fill on its own. */
        blur,
        /** The cover repeated across the frame. */
        tiled,
    };

    struct Options
    {
        Style style = Style::coverOnBlur;

        /**
            Blur radius as a fraction of frame height. folder_gen.py uses a
            15px Gaussian on a frame around 1200 tall.
        */
        float blur = 0.0125f;

        /**
            Cover size as a fraction of the source image, not of the frame.
            Sizing off the source is what keeps the cover the same size
            relative to its own detail whatever resolution the art arrives at.
        */
        float coverScale = 0.5f;

        /** How far above centre the cover sits, as a fraction of frame height. */
        float coverRise = 0.2f;

        /** Border drawn behind the cover, as a fraction of frame width. */
        float coverBorder = 0.0015f;

        /** Covers across the frame when tiled. */
        int tileColumns = 5;

        /**
            Pulled towards black. folder_gen.py does not darken at all, so
            this stays off for the ported styles.
        */
        float darken = 0.0f;
    };

    /** Empty image when the source cannot be read. */
    static juce::Image generate(const juce::File& artwork, const Options& options);

    static bool writeTo(const juce::File& target, const juce::File& artwork,
                        const Options& options);

    /** Style names for the dialog, in enum order. */
    static juce::StringArray styleNames();
};
