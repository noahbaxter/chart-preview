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

juce::StringArray BackgroundGenerator::styleNames()
{
    return { "Cover", "Blur", "Tiled" };
}

juce::Image BackgroundGenerator::generate(const juce::File& artwork, const Options& options)
{
    auto source = juce::ImageFileFormat::loadFrom(artwork);
    if (!source.isValid()) return {};

    // The frame is sized off the source rather than fixed, which is what
    // folder_gen.py does: 16:9 around the art's own resolution, capped.
    int width, height;
    if (source.getWidth() > source.getHeight())
    {
        width = source.getWidth();
        height = juce::roundToInt((float)width * 9.0f / 16.0f);
    }
    else
    {
        height = source.getHeight();
        width = juce::roundToInt((float)height * 16.0f / 9.0f);
    }

    const float capped = juce::jmin(1.0f, (float)kMaxWidth / (float)width,
                                          (float)kMaxHeight / (float)height);
    width = juce::roundToInt((float)width * capped);
    height = juce::roundToInt((float)height * capped);

    juce::Image background(juce::Image::ARGB, width, height, true);

    if (options.style == Style::tiled)
    {
        juce::Graphics g(background);
        const int columns = juce::jmax(1, options.tileColumns);
        const float tile = (float)width / (float)columns;
        for (float y = 0.0f; y < (float)height; y += tile)
            for (float x = 0.0f; x < (float)width; x += tile)
                g.drawImage(source, juce::Rectangle<float>(x, y, tile, tile),
                            juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
    }
    else
    {
        {
            juce::Graphics g(background);
            // Cover, not contain: a square cover on a 16:9 frame has to
            // overflow top and bottom or there would be bars down the sides.
            g.drawImage(source, juce::Rectangle<float>(0.0f, 0.0f, (float)width, (float)height),
                        juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
        }

        boxBlur(background, juce::roundToInt((float)height * juce::jlimit(0.0f, 0.2f, options.blur)));

        if (options.style == Style::coverOnBlur)
        {
            // Scaled off the source image, not the frame, and raised above
            // centre so the highway comes up out of the cover rather than
            // through the middle of it.
            const float scale = juce::jlimit(0.05f, 1.0f, options.coverScale) * capped;
            const int coverWidth = juce::roundToInt((float)source.getWidth() * scale);
            const int coverHeight = juce::roundToInt((float)source.getHeight() * scale);
            const int x = (width - coverWidth) / 2;
            const int y = (height - coverHeight) / 2 - juce::roundToInt((float)height * options.coverRise);
            const int border = juce::roundToInt((float)width * options.coverBorder);

            juce::Graphics g(background);
            g.setColour(juce::Colours::black);
            g.fillRect(juce::Rectangle<int>(x, y, coverWidth, coverHeight).expanded(border));
            g.drawImage(source, juce::Rectangle<int>(x, y, coverWidth, coverHeight).toFloat(),
                        juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
        }
    }

    if (options.darken > 0.0f)
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
