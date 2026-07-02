/*
    ==============================================================================

        GemArtCommon.cpp
        Author: Noah Baxter

    ==============================================================================
*/

#include "GemArtCommon.h"

namespace GemArt
{

float blendOverlay(float backdrop, float source)
{
    return backdrop <= 0.5f ? 2.0f * backdrop * source
                            : 1.0f - 2.0f * (1.0f - backdrop) * (1.0f - source);
}

// W3C non-separable "color" blend: keep the backdrop's luminosity, take the
// source's hue/saturation. Lum/ClipColor/SetLum per the compositing spec.
namespace
{
    struct RGB { float r, g, b; };

    float lum(const RGB& c)      { return 0.3f * c.r + 0.59f * c.g + 0.11f * c.b; }

    RGB clipColour(RGB c)
    {
        float l = lum(c);
        float n = juce::jmin(c.r, c.g, c.b);
        float x = juce::jmax(c.r, c.g, c.b);
        if (n < 0.0f)
        {
            c.r = l + (c.r - l) * l / (l - n);
            c.g = l + (c.g - l) * l / (l - n);
            c.b = l + (c.b - l) * l / (l - n);
        }
        if (x > 1.0f)
        {
            c.r = l + (c.r - l) * (1.0f - l) / (x - l);
            c.g = l + (c.g - l) * (1.0f - l) / (x - l);
            c.b = l + (c.b - l) * (1.0f - l) / (x - l);
        }
        return c;
    }

    RGB setLum(RGB c, float l)
    {
        float d = l - lum(c);
        c.r += d; c.g += d; c.b += d;
        return clipColour(c);
    }
}

juce::Colour blendColour(juce::Colour backdrop, juce::Colour source)
{
    RGB b { backdrop.getFloatRed(), backdrop.getFloatGreen(), backdrop.getFloatBlue() };
    RGB s { source.getFloatRed(),   source.getFloatGreen(),   source.getFloatBlue() };
    RGB r = setLum(s, lum(b));
    return juce::Colour::fromFloatRGBA(r.r, r.g, r.b, backdrop.getFloatAlpha());
}

void applyTintLayers(juce::Image& img, juce::Rectangle<float> contentPx,
                     const std::vector<TintLayer>& layers)
{
    juce::Image::BitmapData bd(img, juce::Image::BitmapData::readWrite);

    for (const auto& layer : layers)
    {
        auto area = juce::Rectangle<float>(
            contentPx.getX() + layer.areaFrac.getX() * contentPx.getWidth(),
            contentPx.getY() + layer.areaFrac.getY() * contentPx.getHeight(),
            layer.areaFrac.getWidth()  * contentPx.getWidth(),
            layer.areaFrac.getHeight() * contentPx.getHeight());

        int x0 = juce::jmax(0, (int) std::floor(area.getX()));
        int y0 = juce::jmax(0, (int) std::floor(area.getY()));
        int x1 = juce::jmin(img.getWidth(),  (int) std::ceil(area.getRight()));
        int y1 = juce::jmin(img.getHeight(), (int) std::ceil(area.getBottom()));

        float sr = layer.colour.getFloatRed();
        float sg = layer.colour.getFloatGreen();
        float sb = layer.colour.getFloatBlue();

        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x)
            {
                auto base = bd.getPixelColour(x, y);
                auto a = base.getAlpha();
                if (a == 0) continue;

                juce::Colour blended;
                switch (layer.mode)
                {
                    case Blend::Overlay:
                        blended = juce::Colour::fromFloatRGBA(
                            blendOverlay(base.getFloatRed(),   sr),
                            blendOverlay(base.getFloatGreen(), sg),
                            blendOverlay(base.getFloatBlue(),  sb),
                            base.getFloatAlpha());
                        break;
                    case Blend::ColourBlend:
                        blended = blendColour(base, layer.colour);
                        break;
                    case Blend::Normal:
                        blended = layer.colour.withAlpha(base.getFloatAlpha());
                        break;
                }

                if (layer.opacity < 1.0f)
                    blended = base.interpolatedWith(blended, layer.opacity);

                bd.setPixelColour(x, y, blended.withAlpha(a));
            }
    }
}

} // namespace GemArt
