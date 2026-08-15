#include "BackgroundGenerator.h"

namespace
{
    /**
        Box blur, run three times.

        Three passes of a box approximate a Gaussian closely enough that the
        difference is invisible at this scale, and they cost a fraction of a
        real Gaussian on a 1920x1080 frame. JUCE's ImageConvolutionKernel would
        need a kernel the size of the radius, which at these radii is enormous.
    */
    void boxBlur(juce::Image& image, int radius)
    {
        if (radius < 1) return;

        const int width = image.getWidth();
        const int height = image.getHeight();
        juce::Image::BitmapData data(image, juce::Image::BitmapData::readWrite);

        std::vector<juce::uint8> line((size_t)juce::jmax(width, height) * 4);

        auto blurRun = [radius](juce::uint8* pixels, int count, int stride)
        {
            std::vector<int> sum(4, 0);
            std::vector<juce::uint8> out((size_t)count * 4);

            const int window = radius * 2 + 1;
            for (int channel = 0; channel < 4; ++channel)
            {
                int running = 0;
                // Edges clamp rather than wrap, so the frame does not pick up
                // a bright fringe from the opposite side.
                for (int i = -radius; i <= radius; ++i)
                    running += pixels[(size_t)juce::jlimit(0, count - 1, i) * (size_t)stride + (size_t)channel];

                for (int i = 0; i < count; ++i)
                {
                    out[(size_t)i * 4 + (size_t)channel] = (juce::uint8)(running / window);
                    const int leaving = juce::jlimit(0, count - 1, i - radius);
                    const int entering = juce::jlimit(0, count - 1, i + radius + 1);
                    running += pixels[(size_t)entering * (size_t)stride + (size_t)channel]
                             - pixels[(size_t)leaving * (size_t)stride + (size_t)channel];
                }
            }

            for (int i = 0; i < count; ++i)
                for (int channel = 0; channel < 4; ++channel)
                    pixels[(size_t)i * (size_t)stride + (size_t)channel] = out[(size_t)i * 4 + (size_t)channel];
        };

        for (int pass = 0; pass < 3; ++pass)
        {
            for (int y = 0; y < height; ++y)
                blurRun(data.getLinePointer(y), width, data.pixelStride);

            for (int x = 0; x < width; ++x)
            {
                // Columns are not contiguous, so they are gathered, blurred,
                // and put back rather than blurred in place.
                for (int y = 0; y < height; ++y)
                    std::memcpy(&line[(size_t)y * 4], data.getPixelPointer(x, y), 4);
                blurRun(line.data(), height, 4);
                for (int y = 0; y < height; ++y)
                    std::memcpy(data.getPixelPointer(x, y), &line[(size_t)y * 4], 4);
            }
        }
    }
}

juce::Image BackgroundGenerator::generate(const juce::File& artwork, const Options& options)
{
    auto source = juce::ImageFileFormat::loadFrom(artwork);
    if (!source.isValid()) return {};

    juce::Image background(juce::Image::ARGB, kWidth, kHeight, true);
    {
        juce::Graphics g(background);

        // Cover, not contain: a square cover on a 16:9 frame has to overflow
        // top and bottom or there would be bars down the sides.
        g.drawImage(source, juce::Rectangle<float>(0.0f, 0.0f, (float)kWidth, (float)kHeight),
                    juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
    }

    boxBlur(background, juce::roundToInt((float)kHeight * juce::jlimit(0.0f, 0.2f, options.blur)));

    {
        juce::Graphics g(background);
        g.setColour(juce::Colours::black.withAlpha(juce::jlimit(0.0f, 1.0f, options.darken)));
        g.fillAll();
    }

    return background;
}

bool BackgroundGenerator::writeTo(const juce::File& target, const juce::File& artwork,
                                  const Options& options)
{
    auto image = generate(artwork, options);
    if (!image.isValid()) return false;

    target.deleteFile();
    juce::FileOutputStream stream(target);
    if (!stream.openedOk()) return false;

    juce::PNGImageFormat png;
    return png.writeImageToStream(image, stream);
}
