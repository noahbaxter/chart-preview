/*
    ==============================================================================

        CymbalGemArt.cpp
        Author: Noah Baxter

        A cymbal is a stack of concentric discs (a shallow bell on a grooved
        saucer). It is drawn painter's-style, back to front: the widest, lowest
        disc first, then each smaller/higher disc on top. Because of the view
        angle you only ever see the BOTTOM arc of each oval, so a black disc
        drawn between two colored discs shows only as a bottom crescent once the
        next disc is painted over it, which is exactly how the real grooves
        read. Ellipse centres/radii are measured from cym_blue.png. Colors are
        derived from one lane colour, so the whole cymbal tints (incl. purple);
        the cone keeps an angular metallic sheen. Geometry is in the original
        1194x598 canvas, mapped onto contentBounds so sizing is unchanged.

    ==============================================================================
*/

#include "CymbalGemArt.h"

namespace GemArt
{

namespace
{
    constexpr float kSrcW = 1194.0f;
    constexpr float kSrcY0 = 106.0f, kSrcH = 385.0f;
    constexpr float kCx = 596.0f;

    // Disc ovals taken straight from the source vector art (cym blue.pdf,
    // converted to SVG, ellipse anchors mapped PDF->PNG by the uniform affine
    // s=8.429, oy=107.5 that lands every bottom crossing). These are the TRUE
    // disc centres, not the visible-crescent centres. Painter's order outer ->
    // inner; ry = bottomY - cy. Consecutive discs sharing rx are vertical steps
    // (the grooves), which is what makes the black overhang read right.
    struct Ell { float rx, cy, bottomY; };
    constexpr Ell kBrim  { 593.0f, 333.0f, 490.0f };   // white brim (lip)
    constexpr Ell kSkirt { 566.0f, 294.0f, 420.0f };   // dark skirt
    constexpr Ell kG2    { 429.0f, 289.0f, 347.0f };   // groove 2 (black step)
    constexpr Ell kRim   { 429.0f, 245.0f, 334.0f };   // light rim
    constexpr Ell kG1    { 327.0f, 202.0f, 289.0f };   // groove 1 (black step)
    constexpr Ell kCone  { 327.0f, 194.0f, 281.0f };   // cone base disc (vector #9)

    constexpr float kApexX = 596.0f, kApexY = 106.0f;

    // Cone metallic sheen by ray angle from apex (deg, 0=+x, 90=down), measured.
    struct AngLum { float deg, lum; };
    constexpr AngLum kConeP[] = {
        {  12.0f,  47 }, {  16.0f,  40 }, {  20.0f,  73 }, {  24.0f, 114 },
        {  28.0f, 149 }, {  32.0f, 134 }, {  36.0f,  99 }, {  40.0f,  61 },
        {  44.0f,  37 }, {  48.0f,  32 }, {  52.0f,  37 }, {  56.0f,  43 },
        {  60.0f,  51 }, {  64.0f,  59 }, {  68.0f,  68 }, {  72.0f,  77 },
        {  76.0f,  87 }, {  80.0f,  98 }, {  84.0f, 106 }, {  88.0f, 117 },
        {  92.0f, 125 }, {  96.0f, 133 }, { 100.0f, 145 }, { 104.0f, 160 },
        { 108.0f, 176 }, { 112.0f, 194 }, { 116.0f, 218 }, { 120.0f, 245 },
        { 124.0f, 255 }, { 144.0f, 255 }, { 148.0f, 247 }, { 152.0f, 197 },
        { 156.0f, 142 }, { 160.0f, 103 }, { 164.0f,  71 }, { 168.0f,  56 },
    };
    float coneLumAt(float deg)
    {
        const int n = (int) (sizeof(kConeP) / sizeof(kConeP[0]));
        if (deg <= kConeP[0].deg || deg >= kConeP[n - 1].deg) return 34.0f;
        for (int i = 1; i < n; ++i)
            if (deg <= kConeP[i].deg)
            {
                float f = (deg - kConeP[i - 1].deg) / (kConeP[i].deg - kConeP[i - 1].deg);
                return kConeP[i - 1].lum + (kConeP[i].lum - kConeP[i - 1].lum) * f;
            }
        return kConeP[n - 1].lum;
    }
    juce::Colour coneColour(float lum, juce::Colour base)
    {
        float hinge = 0.3f * base.getRed() + 0.59f * base.getGreen() + 0.11f * base.getBlue();
        hinge = juce::jlimit(40.0f, 200.0f, hinge);
        if (lum <= hinge)
            return base.withMultipliedBrightness(0.42f).interpolatedWith(base, lum / hinge);
        return base.interpolatedWith(juce::Colours::white, (lum - hinge) / (255.0f - hinge));
    }

    float ryOf(const Ell& e)  { return e.bottomY - e.cy; }
    float cyOf(const Ell& e)  { return e.cy; }
    juce::Rectangle<float> rectOf(const Ell& e)
    {
        float ry = ryOf(e);
        return { kCx - e.rx, e.cy - ry, e.rx * 2.0f, ry * 2.0f };
    }
}

CymbalStyle cymbalStyle(juce::Colour lane)
{
    return { lane, juce::Colour(0x00000000), juce::Colour(0xfff0f2f5) };  // brim: soft off-white
}

CymbalStyle cymbalStyleWhite()
{
    return { juce::Colour(0xffb8bcc0),
             juce::Colour(0xffe0a94a),      // gold rim (cym_white.png)
             juce::Colour(0xfff2f2f2) };
}

juce::Image bakeCymbal(const CymbalStyle& style,
                       juce::Rectangle<int> canvas,
                       juce::Rectangle<int> contentBounds)
{
    juce::Image img(juce::Image::ARGB, canvas.getWidth(), canvas.getHeight(), true);

    float sx = contentBounds.getWidth() / kSrcW;
    float sy = contentBounds.getHeight() / kSrcH;
    auto toDev = juce::AffineTransform::translation(0.0f, -kSrcY0)
                     .scaled(sx, sy)
                     .translated((float) contentBounds.getX(), (float) contentBounds.getY());

    auto base   = style.base;
    // rim measured front = (110,137,187): lane lifted toward white AND
    // desaturated (matte), so it stays subtler than the skirt.
    auto rimCol = style.rim.getARGB() != 0 ? style.rim
                    : base.interpolatedWith(juce::Colours::white, 0.34f).withMultipliedSaturation(0.62f);
    auto skirtDark = base.withMultipliedBrightness(0.55f);   // shaded back/top of skirt
    auto rimDark   = rimCol.withMultipliedBrightness(0.72f);

    // Painter's stack: outer/lowest first, each drawn on top -> bottom crescents.
    {
        juce::Graphics g(img);
        g.addTransform(toDev);

        // Brim (white / gold), full oval.
        g.setColour(style.lip);
        g.fillEllipse(rectOf(kBrim));

        // Skirt: vertical gradient, dark at back (top) -> lit at front (bottom).
        {
            auto r = rectOf(kSkirt);
            juce::ColourGradient grad(skirtDark, kCx, r.getY(), base, kCx, r.getBottom(), false);
            g.setGradientFill(grad);
            g.fillEllipse(r);
        }

        // Groove 2 (black), covered by rim -> bottom crescent.
        g.setColour(juce::Colours::black);
        g.fillEllipse(rectOf(kG2));

        // Rim: vertical gradient, slightly shaded at back.
        {
            auto r = rectOf(kRim);
            juce::ColourGradient grad(rimDark, kCx, r.getY(), rimCol, kCx, r.getBottom(), false);
            g.setGradientFill(grad);
            g.fillEllipse(r);
        }

        // Groove 1 (black), covered by cone -> bottom crescent.
        g.setColour(juce::Colours::black);
        g.fillEllipse(rectOf(kG1));
    }

    // Cone: fill the whole base ellipse (a metallic disc whose radial spokes +
    // apex hot spot read as a cap). Filling the full disc - not a narrow fan -
    // means it occludes groove1's top, leaving groove1 as a bottom crescent.
    {
        juce::Image::BitmapData bd(img, juce::Image::BitmapData::readWrite);
        auto inv = toDev.inverted();
        float cbCy = cyOf(kCone), cbRx = kCone.rx, cbRy = ryOf(kCone);

        auto bounds = rectOf(kCone).transformedBy(toDev);
        int px0 = juce::jmax(0, (int) bounds.getX() - 2);
        int px1 = juce::jmin(canvas.getWidth(),  (int) bounds.getRight() + 2);
        int py0 = juce::jmax(0, (int) bounds.getY() - 2);
        int py1 = juce::jmin(canvas.getHeight(), (int) bounds.getBottom() + 2);

        for (int py = py0; py < py1; ++py)
            for (int px = px0; px < px1; ++px)
            {
                auto p = juce::Point<float>(px + 0.5f, py + 0.5f).transformedBy(inv);
                float ex = (p.x - kCx) / cbRx, ey = (p.y - cbCy) / cbRy;
                if (ex * ex + ey * ey > 1.0f) continue;   // outside the base disc

                float dx = p.x - kApexX, dy = p.y - kApexY;
                float deg = juce::radiansToDegrees(std::atan2(dy, dx));
                if (deg < 0.0f) deg += 360.0f;
                float lum = coneLumAt(deg);
                // 3D dome: normalised radius from apex to the base rim along
                // this ray (0 at apex, 1 at the base edge). Bright peak at the
                // apex, gently darkening toward the rim gives the cone form.
                float rr = std::sqrt(ex * ex + ey * ey);        // 0..1 in base ellipse
                float apexDist = std::sqrt(dx * dx + dy * dy) / (2.0f * cbRy);
                if (apexDist < 0.22f)
                    lum += (255.0f - lum) * (1.0f - apexDist / 0.22f) * 0.9f;
                lum *= 1.0f - 0.28f * rr * rr;                  // darken toward the rim
                bd.setPixelColour(px, py, coneColour(lum, base));
            }
    }

    return img;
}

} // namespace GemArt
