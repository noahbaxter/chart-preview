/*
    ==============================================================================

        NoteGemArt.cpp
        Author: Noah Baxter

        Layer recipe and every gradient stop below are read straight from the
        source SVGs (assets/archive/svg/), not eyeballed. Coordinates are in
        viewBox units; the Graphics transform maps them onto contentBounds.

    ==============================================================================
*/

#include "NoteGemArt.h"

namespace GemArt
{

TubeGemGeom noteGeom()
{
    TubeGemGeom g;
    g.vbW = 126.02f;   g.vbH = 37.999f;
    g.capOuterW = 16.042f;  g.capOuterR = 10.132f;
    g.insetX = 2.0f;        g.insetTopY = 4.882f;   g.insetR = 8.038f;
    g.chromeTopY = 13.882f; g.chromeR = 9.663f;
    g.pillarW = 2.967f;
    g.artBottom = 33.965f;
    g.sheenBottom = 4.882f;
    g.streakTop = 13.882f;  g.streakBottom = 23.237f;
    g.radCx = 63.343f; g.radCy = 19.427f; g.radR = 59.718f;
    g.radTx = 0.0f; g.radTy = 10.017f; g.radSx = 1.0f; g.radSy = 0.484f;
    return g;
}

TubeGemGeom hopoGeom()
{
    TubeGemGeom g;
    g.vbW = 76.437f;   g.vbH = 30.891f;
    g.capOuterW = 13.041f;  g.capOuterR = 8.237f;
    g.insetX = 1.626f;      g.insetTopY = 3.969f;   g.insetR = 6.535f;
    g.chromeTopY = 11.285f; g.chromeR = 7.856f;
    g.pillarW = 8.098f;
    g.artBottom = 27.612f;
    g.sheenBottom = 3.969f;
    g.streakTop = 11.285f;  g.streakBottom = 18.89f;
    g.radCx = 38.391f; g.radCy = 15.793f; g.radR = 33.084f;
    g.radTx = 9.546f; g.radTy = 8.144f; g.radSx = 0.75f; g.radSy = 0.484f;
    return g;
}

TubeGemStyle tubeStyleOverlay(juce::Colour lane)
{
    return { { { lane, Blend::Overlay, 1.0f } }, false };
}

TubeGemStyle tubeStyleGreen()
{
    return { { { juce::Colour(0xff39b54a), Blend::ColourBlend, 1.0f } }, false };
}

TubeGemStyle tubeStyleOrange()
{
    return { { { juce::Colour(0xffff7100), Blend::ColourBlend, 1.0f },
               { juce::Colour(0xffffcd00), Blend::Overlay, 0.3f } }, false };
}

TubeGemStyle tubeStyleWhite()
{
    return { { { juce::Colours::white, Blend::Overlay, 0.75f } }, true };
}

namespace
{
    // Left outer cap: rounded top-left corner, flat everywhere else.
    juce::Path capOuterPath(const TubeGemGeom& gg)
    {
        juce::Path p;
        p.startNewSubPath(0.0f, gg.capOuterR);
        p.lineTo(0.0f, gg.artBottom);
        p.lineTo(gg.capOuterW, gg.artBottom);
        p.lineTo(gg.capOuterW, 0.0f);
        p.lineTo(gg.capOuterR, 0.0f);
        p.addArc(0.0f, 0.0f, gg.capOuterR * 2.0f, gg.capOuterR * 2.0f,
                 0.0f, -juce::MathConstants<float>::halfPi, false);
        p.closeSubPath();
        return p;
    }

    juce::Path capInsetPath(const TubeGemGeom& gg)
    {
        juce::Path p;
        float top = gg.insetTopY, r = gg.insetR, x0 = gg.insetX;
        p.startNewSubPath(x0, top + r);
        p.lineTo(x0, gg.artBottom);
        p.lineTo(gg.capOuterW, gg.artBottom);
        p.lineTo(gg.capOuterW, top);
        p.lineTo(x0 + r, top);
        p.addArc(x0, top, r * 2.0f, r * 2.0f,
                 0.0f, -juce::MathConstants<float>::halfPi, false);
        p.closeSubPath();
        return p;
    }

    juce::Path capChromePath(const TubeGemGeom& gg)
    {
        juce::Path p;
        float top = gg.chromeTopY, r = gg.chromeR, x0 = gg.insetX;
        p.startNewSubPath(x0, top + r);
        p.lineTo(x0, gg.artBottom);
        p.lineTo(gg.capOuterW, gg.artBottom);
        p.lineTo(gg.capOuterW, top);
        p.lineTo(x0 + r, top);
        p.addArc(x0, top, r * 2.0f, r * 2.0f,
                 0.0f, -juce::MathConstants<float>::halfPi, false);
        p.closeSubPath();
        return p;
    }

    juce::ColourGradient withStops(juce::Point<float> p1, juce::Point<float> p2,
                                   std::initializer_list<std::pair<double, juce::uint32>> stops,
                                   bool radial = false)
    {
        auto it = stops.begin();
        juce::ColourGradient grad(juce::Colour(it->second), p1.x, p1.y,
                                  juce::Colour((stops.end() - 1)->second), p2.x, p2.y,
                                  radial);
        grad.clearColours();
        for (auto& s : stops)
            grad.addColour(s.first, juce::Colour(s.second));
        return grad;
    }

    // linear-gradient-2: inner cap chrome, vertical
    juce::ColourGradient chromeGradient(const TubeGemGeom& gg)
    {
        return withStops({ 0.0f, gg.chromeTopY }, { 0.0f, gg.artBottom },
            { { 0.0,  0xfff1f2f2 }, { 0.218, 0xffbfbfc0 }, { 0.481, 0xff89898a },
              { 0.709, 0xff616162 }, { 0.889, 0xff49484a }, { 1.0,  0xff414042 } });
    }

    // linear-gradient-5: pillar chrome, vertical, all 22 stops verbatim
    juce::ColourGradient pillarGradient(const TubeGemGeom& gg)
    {
        return withStops({ 0.0f, 0.0f }, { 0.0f, gg.artBottom },
            { { 0.0,  0xffffffff }, { 0.06,  0xfffdfdfd }, { 0.081, 0xfff6f6f6 },
              { 0.097, 0xffeaeaea }, { 0.109, 0xffd9d9da }, { 0.12,  0xffc3c3c4 },
              { 0.129, 0xffa8a8a9 }, { 0.138, 0xff87888a }, { 0.145, 0xff636466 },
              { 0.148, 0xff58595b }, { 0.223, 0xff5a5b5d }, { 0.264, 0xff626365 },
              { 0.297, 0xff6f7072 }, { 0.325, 0xff838385 }, { 0.35,  0xff9c9c9e },
              { 0.373, 0xffbbbbbc }, { 0.394, 0xffdedfdf }, { 0.41,  0xffffffff },
              { 0.559, 0xffdbdcdd }, { 0.752, 0xffb4b5b7 }, { 0.906, 0xff9b9da0 },
              { 1.0,   0xff939598 } });
    }

    // linear-gradient-7: light streak, bottom -> top
    juce::ColourGradient streakGradient(const TubeGemGeom& gg)
    {
        return withStops({ 0.0f, gg.streakBottom }, { 0.0f, gg.streakTop },
            { { 0.0,  0xff808285 }, { 0.103, 0xff828487 }, { 0.165, 0xff8a8c8f },
              { 0.216, 0xff999b9d }, { 0.261, 0xffadafb0 }, { 0.302, 0xffc7c9ca },
              { 0.34,  0xffe7e8e8 }, { 0.35,  0xfff1f2f2 }, { 0.55,  0xfff1f2f2 },
              { 0.562, 0xffe7e8e8 }, { 0.611, 0xffc7c9ca }, { 0.664, 0xffadafb0 },
              { 0.722, 0xff999b9d }, { 0.788, 0xff8a8c8f }, { 0.868, 0xff828487 },
              { 1.0,   0xff808285 } });
    }

    // linear-gradient-8: bottom bar, horizontal
    juce::ColourGradient bottomBarGradient(const TubeGemGeom& gg)
    {
        return withStops({ gg.vbW, 0.0f }, { 0.0f, 0.0f },
            { { 0.0,  0xff414042 }, { 0.019, 0xff464547 }, { 0.043, 0xff565658 },
              { 0.07,  0xff717173 }, { 0.1,   0xff979798 }, { 0.131, 0xffc6c6c8 },
              { 0.15,  0xffe6e7e8 }, { 0.85,  0xffe6e7e8 }, { 0.869, 0xffc6c6c8 },
              { 0.9,   0xff979798 }, { 0.93,  0xff717173 }, { 0.957, 0xff565658 },
              { 0.981, 0xff464547 }, { 1.0,   0xff414042 } });
    }
}

juce::Image bakeTubeGem(const TubeGemGeom& gg,
                        juce::Rectangle<int> canvas,
                        juce::Rectangle<int> contentBounds,
                        const TubeGemStyle& style)
{
    juce::Image img(juce::Image::ARGB, canvas.getWidth(), canvas.getHeight(), true);
    juce::Graphics g(img);

    // Map viewBox units onto the content rect; gradients follow the transform.
    auto toContent = juce::AffineTransform::scale(contentBounds.getWidth() / gg.vbW,
                                                  contentBounds.getHeight() / gg.vbH)
                         .translated((float) contentBounds.getX(),
                                     (float) contentBounds.getY());
    g.addTransform(toContent);

    const float bodyX0 = gg.capOuterW + gg.pillarW;
    const float bodyX1 = gg.vbW - bodyX0;
    const auto mirror = juce::AffineTransform::scale(-1.0f, 1.0f).translated(gg.vbW, 0.0f);

    // 1-3. Left cap: outer silver, black inset, inner chrome
    {
        auto outer = capOuterPath(gg);
        auto inset = capInsetPath(gg);
        auto chrome = capChromePath(gg);

        g.setGradientFill(withStops({ gg.capOuterW, 0.0f }, { 0.0f, 0.0f },
            { { 0.0, 0xffa7a9ac }, { 1.0, 0xff414042 } }));
        g.fillPath(outer);
        g.setColour(juce::Colours::black);
        g.fillPath(inset);
        g.setGradientFill(chromeGradient(gg));
        g.fillPath(chrome);

        // 4. Right cap, mirrored (gradients re-specified mirrored via the path transform)
        juce::Path outerR = outer, insetR = inset, chromeR = chrome;
        outerR.applyTransform(mirror);
        insetR.applyTransform(mirror);
        chromeR.applyTransform(mirror);
        g.setGradientFill(withStops({ gg.vbW - gg.capOuterW, 0.0f }, { gg.vbW, 0.0f },
            { { 0.0, 0xffa7a9ac }, { 1.0, 0xff414042 } }));
        g.fillPath(outerR);
        g.setColour(juce::Colours::black);
        g.fillPath(insetR);
        g.setGradientFill(chromeGradient(gg));
        g.fillPath(chromeR);
    }

    // 5. Pillars
    g.setGradientFill(pillarGradient(gg));
    g.fillRect(juce::Rectangle<float>(gg.capOuterW, 0.0f, gg.pillarW, gg.artBottom));
    g.fillRect(juce::Rectangle<float>(bodyX1, 0.0f, gg.pillarW, gg.artBottom));

    // 6. Body: radial greyscale, y-squashed per the SVG gradientTransform
    {
        juce::Graphics::ScopedSaveState save(g);
        auto squash = juce::AffineTransform::scale(gg.radSx, gg.radSy)
                          .translated(gg.radTx, gg.radTy);
        juce::Path body;
        body.addRectangle(bodyX0, 0.0f, bodyX1 - bodyX0, gg.artBottom);
        body.applyTransform(squash.inverted());
        g.addTransform(squash);
        juce::ColourGradient rad(juce::Colour(0xff939598), gg.radCx, gg.radCy,
                                 juce::Colours::black, gg.radCx + gg.radR, gg.radCy, true);
        g.setGradientFill(rad);
        g.fillPath(body);
    }

    // 7. Top sheen
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.fillRect(juce::Rectangle<float>(bodyX0, 0.0f, bodyX1 - bodyX0, gg.sheenBottom));

    // 8. Light streak (element opacity .9 in the SVG)
    {
        g.beginTransparencyLayer(0.9f);
        g.setGradientFill(streakGradient(gg));
        g.fillRect(juce::Rectangle<float>(bodyX0, gg.streakTop,
                                          bodyX1 - bodyX0, gg.streakBottom - gg.streakTop));
        g.endTransparencyLayer();
    }

    // 9. Bottom bar
    g.setGradientFill(bottomBarGradient(gg));
    g.fillRect(juce::Rectangle<float>(0.0f, gg.artBottom, gg.vbW, gg.vbH - gg.artBottom));

    // 10. Tint layers (per-pixel blend pass, original layer order). Body tints
    // cover the body rect; the OD variant additionally overlays gold on the
    // caps + pillars (chrome structure shows through, per the original render).
    {
        auto content = contentBounds.toFloat();
        std::vector<TintLayer> layers;

        if (style.capsGold)
        {
            const juce::Colour gold(0xffffb500);
            float capsW = bodyX0 / gg.vbW;
            float artH  = gg.artBottom / gg.vbH;
            layers.push_back({ gold, Blend::Overlay, 1.0f, { 0.0f, 0.0f, capsW, artH } });
            layers.push_back({ gold, Blend::Overlay, 1.0f, { 1.0f - capsW, 0.0f, capsW, artH } });
        }

        for (auto l : style.tints)
        {
            l.areaFrac = { bodyX0 / gg.vbW, 0.0f,
                           (bodyX1 - bodyX0) / gg.vbW, gg.artBottom / gg.vbH };
            layers.push_back(l);
        }

        if (! layers.empty())
            applyTintLayers(img, content, layers);
    }

    return img;
}

} // namespace GemArt
