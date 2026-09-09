/*
    ==============================================================================

        BarGemArt.cpp
        Author: Noah Baxter

        Geometry measured from bar_white.png (2432x152, content 86..2346):
        tube height 98, arc top y = 37 * t^2 with t = (x - 1216) / 1080,
        caps ~75 wide leaning ~27 degrees, two-lobe end shading. Colour ramps
        are sampled centre columns of each original bar.

    ==============================================================================
*/

#include "BarGemArt.h"
#include "../Geometry/PositionConstants.h"

namespace GemArt
{

namespace
{
    // Measured in original-pixel units on the 2260x152 content box.
    constexpr float kSrcW        = 2260.0f;
    constexpr float kSrcH        = 152.0f;
    constexpr float kTubeH       = 98.0f;
    constexpr float kArcCentre   = 1130.0f;  // abs 1216 - content x0 86
    constexpr float kArcHalf     = 1080.0f;
    // Extra top-y at t = +/-1, derived (span * curvature) the way bakeGridline's arch is,
    // so the bar tracks the note curvature instead of the 37.0 measured off bar_white.png.
    constexpr float kCurv        = PositionConstants::NOTE_CURVATURE_DRUMS < 0.0f
                                 ? -PositionConstants::NOTE_CURVATURE_DRUMS
                                 :  PositionConstants::NOTE_CURVATURE_DRUMS;
    constexpr float kArch        = 2.0f * kArcHalf * kCurv;
    constexpr float kTubeX0      = 40.0f;    // tube runs under the caps
    constexpr float kAccentLineInset = 120.0f;  // clears the leaning end caps

    BarRamp makeRamp(std::initializer_list<std::pair<float, juce::uint32>> stops)
    {
        BarRamp r;
        for (auto& s : stops)
            r.push_back({ s.first, juce::Colour(s.second) });
        return r;
    }

    juce::Colour rampAt(const BarRamp& ramp, float t)
    {
        if (t <= ramp.front().first) return ramp.front().second;
        for (size_t i = 1; i < ramp.size(); ++i)
            if (t <= ramp[i].first)
            {
                float span = ramp[i].first - ramp[i - 1].first;
                float f = span > 0.0f ? (t - ramp[i - 1].first) / span : 0.0f;
                return ramp[i - 1].second.interpolatedWith(ramp[i].second, f);
            }
        return ramp.back().second;
    }

    float arcTopAt(float x)
    {
        float t = (x - kArcCentre) / kArcHalf;
        return kArch * t * t;
    }

    // Inverse of squeezing the bar to `thickness` about the tube's centreline at x. Callers
    // sample in squeezed space and un-squeeze to test against the full-thickness geometry,
    // so thickness never touches arcTopAt: a thin bar keeps the fat bar's arc and centreline.
    float unsqueezeY(float x, float y, float thickness)
    {
        const float cy = arcTopAt(x) + kTubeH * 0.5f;
        return cy + (y - cy) / thickness;
    }

    // Left cap outline in content units, measured row-by-row from bar_white:
    // a leaning slab, narrow rounded tip up top, widening toward a broadly
    // rounded bottom-left corner.
    juce::Path capPath()
    {
        juce::Path p;
        p.startNewSubPath(14.0f, 152.0f);                       // bottom edge, right end
        p.quadraticTo(2.0f, 150.0f, 0.0f, 136.0f);              // bottom-left round
        // Left edge through the measured row samples (steep near the tip,
        // flattening toward the bottom bulge)
        p.lineTo(2.0f, 120.0f);
        p.lineTo(9.0f, 100.0f);
        p.lineTo(16.0f, 80.0f);
        p.lineTo(25.0f, 60.0f);
        p.quadraticTo(31.0f, 47.0f, 39.0f, 40.0f);
        p.quadraticTo(47.0f, 32.0f, 56.0f, 27.0f);
        p.quadraticTo(62.0f, 24.0f, 70.0f, 24.0f);              // rounded tip
        p.quadraticTo(77.0f, 24.5f, 82.0f, 27.0f);              // top edge
        p.lineTo(48.0f, 152.0f);                                // right edge, straight lean
        p.closeSubPath();
        return p;
    }

    // Two stacked lobes shading, expressed as a ramp along the cap's
    // top->bottom axis. The bands are ARCS, not straight lines: the band
    // coordinate is bowed by the across-axis offset (pseudo-3D tube end),
    // so this is evaluated per pixel in shadeCap(), not as a gradient fill.
    const BarRamp& capRamp()
    {
        static const BarRamp ramp = makeRamp({
            { 0.0f,   0xff4f4f4f }, { 0.10f, 0xff7c7c7c }, { 0.157f, 0xff9e9e9e },
            { 0.22f,  0xff6e6e6e }, { 0.33f, 0xff1f1f1f }, { 0.42f,  0xff2e2e2e },
            { 0.447f, 0xff9e9e9e }, { 0.50f, 0xff767676 }, { 0.56f,  0xff454545 },
            { 0.63f,  0xff1f1f1f }, { 1.0f,  0xff1f1f1f } });
        return ramp;
    }

    // Per-pixel cap shading in content units. leftCap mirrors x for the right
    // side. The lobe bands bow toward the cap tip as you move off-axis.
    void shadeCap(juce::Image& img, juce::Rectangle<int> contentBounds, bool leftCap,
                  float thickness)
    {
        auto path = capPath();
        // Nearly vertical axis: bands start ~horizontal at the axis and only
        // gain their tilt from the downward bow toward the edges.
        const juce::Point<float> a(66.0f, 24.0f), b(48.0f, 152.0f);
        auto axis = b - a;
        float len = axis.getDistanceFromOrigin();
        auto dir = axis / len;
        const juce::Point<float> perp(-dir.y, dir.x);
        constexpr float kBandBow = 0.013f;   // px of axis shift per across-px^2

        float sx = contentBounds.getWidth() / kSrcW;
        float sy = contentBounds.getHeight() / kSrcH;

        juce::Image::BitmapData bd(img, juce::Image::BitmapData::readWrite);
        auto bounds = path.getBounds();
        // Pixel x-range: mirror the path bounds for the right cap.
        float bx0 = leftCap ? bounds.getX() : kSrcW - bounds.getRight();
        float bx1 = leftCap ? bounds.getRight() : kSrcW - bounds.getX();
        int px0 = (int) std::floor(bx0 * sx) + contentBounds.getX();
        int px1 = (int) std::ceil(bx1 * sx) + contentBounds.getX();
        int py0 = (int) std::floor(bounds.getY() * sy) + contentBounds.getY();
        int py1 = (int) std::ceil(bounds.getBottom() * sy) + contentBounds.getY();

        for (int py = juce::jmax(0, py0); py < juce::jmin(img.getHeight(), py1); ++py)
            for (int px = juce::jmax(0, px0); px < juce::jmin(img.getWidth(), px1); ++px)
            {
                // 2x2 coverage sampling for an anti-aliased outline
                int hits = 0;
                float cxSrc = 0.0f, cySrc = 0.0f;
                for (int s = 0; s < 4; ++s)
                {
                    float fx = (px + 0.25f + 0.5f * (s & 1) - contentBounds.getX()) / sx;
                    if (! leftCap) fx = kSrcW - fx;
                    float fy = unsqueezeY(fx, (py + 0.25f + 0.5f * (s >> 1) - contentBounds.getY()) / sy,
                                          thickness);
                    if (path.contains(fx, fy)) { ++hits; cxSrc = fx; cySrc = fy; }
                }
                if (hits == 0) continue;

                juce::Point<float> rel(cxSrc - a.x, cySrc - a.y);
                float u = rel.getDotProduct(dir);
                float v = rel.getDotProduct(perp);
                float t = (u - kBandBow * v * v) / len;   // bands bow down toward the edges
                auto col = rampAt(capRamp(), juce::jlimit(0.0f, 1.0f, t));
                if (hits == 4)
                    bd.setPixelColour(px, py, col);
                else
                {
                    auto base = bd.getPixelColour(px, py);
                    float cov = hits * 0.25f;
                    if (base.getAlpha() == 0)
                        bd.setPixelColour(px, py, col.withAlpha(cov));
                    else
                        bd.setPixelColour(px, py, base.interpolatedWith(col, cov));
                }
            }
    }
}

BarRamp barRampWhite()
{
    return makeRamp({
        { 0.000f, 0xff6e6e71 },
        { 0.072f, 0xffa1a1a4 }, { 0.144f, 0xffcdcdce }, { 0.216f, 0xffe8e8e9 },
        { 0.289f, 0xfff9f9f9 }, { 0.361f, 0xfffdfdfd }, { 0.433f, 0xfff3f4f4 },
        { 0.505f, 0xffe5e6e6 }, { 0.577f, 0xffd8d9d9 }, { 0.649f, 0xffcacbcc },
        { 0.722f, 0xffb9babb }, { 0.794f, 0xffa8a9ab }, { 0.866f, 0xff9c9da0 },
        { 0.938f, 0xff8a8c8f }, { 1.000f, 0xff808285 } });
}

BarRamp barRampKick()
{
    return makeRamp({
        { 0.000f, 0xfff0650a }, { 0.072f, 0xfff5930f }, { 0.144f, 0xfff8bc15 },
        { 0.216f, 0xfffbda19 }, { 0.289f, 0xfffef11d }, { 0.361f, 0xfffef81d },
        { 0.433f, 0xfffde51b }, { 0.505f, 0xfff9c417 }, { 0.577f, 0xfff7ae14 },
        { 0.649f, 0xfff59a11 }, { 0.722f, 0xfff38a0f }, { 0.794f, 0xfff27c0d },
        { 0.866f, 0xfff1700b }, { 0.938f, 0xfff0680a }, { 1.000f, 0xfff0650a } });
}

BarRamp barRampKick2x()
{
    return makeRamp({
        { 0.000f, 0xfff0430a }, { 0.072f, 0xfff55f0f }, { 0.144f, 0xfff87b15 },
        { 0.216f, 0xfffb8f19 }, { 0.289f, 0xfffe9e1d }, { 0.361f, 0xfffea21d },
        { 0.433f, 0xfffd961b }, { 0.505f, 0xfff98117 }, { 0.577f, 0xfff77214 },
        { 0.649f, 0xfff56611 }, { 0.722f, 0xfff35b0f }, { 0.794f, 0xfff2510d },
        { 0.866f, 0xfff1490b }, { 0.938f, 0xfff0440a }, { 1.000f, 0xfff0430a } });
}

BarRamp barRampOpen()
{
    return makeRamp({
        { 0.000f, 0xff7200d3 }, { 0.072f, 0xff9828de }, { 0.144f, 0xffb749e8 },
        { 0.216f, 0xffd86cf2 }, { 0.289f, 0xfff287fa }, { 0.361f, 0xfffd93fe },
        { 0.433f, 0xffeb80f8 }, { 0.505f, 0xffcb5fee }, { 0.577f, 0xffb649e8 },
        { 0.649f, 0xffa435e2 }, { 0.722f, 0xff9525dd }, { 0.794f, 0xff8717da },
        { 0.866f, 0xff7c0bd6 }, { 0.938f, 0xff7503d4 }, { 1.000f, 0xff7200d3 } });
}

juce::Image bakeStompBar(float arch, float thickness)
{
    // Flat pseudo-3D model measured off the source: a flat white PLANE (full-width thin
    // trapezoid, tapered ends) with a raised BOX on centre. Two flat greys, NO gradients:
    //   white (255) = plane + box top face; grey (190) = box front; darker grey = box side.
    // `thickness` scales vertical extent; `arch` bows the whole profile down at the ends
    // (parabola) so it follows the curved gridlines.
    constexpr int   W     = 2432;
    const juce::Colour WHITE (0xffffffff);   // plane + box top face  (source 255)
    const juce::Colour GREY  (0xffbebebe);   // box front face        (source 190)
    const juce::Colour CAP   (0xff808080);   // box left/right end caps (source 128)

    // Box geometry (content px): a uniform ~0.70x scale of the measured source (3469x187) so the
    // box keeps its true wide/flat proportion. The box sits ON the flat plane and spans the
    // plane's full depth: its end caps bridge the plane's BACK edge (planeTop) to its FRONT edge
    // (planeBot) as a diagonal, so no flat plane shows in front of or behind the box. The plane
    // is a parallelogram: its FRONT edge is inset by `depth` so both ends read as diagonals (a
    // flat slab in perspective). `depth` = one unit of receding depth, shared by the plane ends
    // and the box caps so they stay consistent. `thickness` scales vertically about the plane
    // centreline; `arch` bows the ends down (parabola) to follow the curved gridlines.
    const float cx        = W * 0.5f;
    const float hw        = 387.0f;    // front-face half width (source 552 * 0.70)
    const float depth     = 22.0f;     // receding-depth X offset (box cap splay + plane end diagonal)
    const float pcy       = 75.0f;     // plane band centreline
    auto sy = [&](float y) { return pcy + (y - pcy) * thickness; };
    const float topBack   = sy(8.0f);    // top-face back edge (highest point)
    const float frontTop  = sy(48.0f);   // top-face / front-face seam
    const float planeTop  = sy(55.0f);   // plane BACK edge  (box back-bottom sits here)
    const float planeBot  = sy(95.0f);   // plane FRONT edge (box front-bottom sits here)
    auto arc = [&](float x) { float t = (x - cx) / cx; return arch * t * t; };

    // Size the canvas around the actual content extent (topBack is the highest point and goes
    // negative once thickness > 1, since sy scales about the plane centreline). Shift content so
    // the top sits at `pad`, with symmetric padding, so the box top never clips and the content
    // stays centred in the image (thickness = 1 -> yShift 0, identical to the old bake).
    const float pad    = 8.0f;
    const float yShift = pad - topBack;
    const int   H = (int) std::ceil((planeBot - topBack) + std::abs(arch) + 2.0f * pad);

    juce::Image img(juce::Image::ARGB, W, H, true);
    juce::Graphics g(img);

    auto quad = [&](juce::Colour c,
                    float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3)
    {
        juce::Path p;
        p.startNewSubPath(x0, y0 + yShift + arc(x0)); p.lineTo(x1, y1 + yShift + arc(x1));
        p.lineTo(x2, y2 + yShift + arc(x2));          p.lineTo(x3, y3 + yShift + arc(x3));
        p.closeSubPath();
        g.setColour(c); g.fillPath(p);
    };

    // Flat white plane: a parallelogram — full-width BACK edge, front edge inset by `depth` so
    // both ends are diagonals (implying the flat slab receding in perspective).
    quad(WHITE, 0.0f, planeTop, (float) W, planeTop, (float) W - depth, planeBot, depth, planeBot);
    const float capW = depth;

    // Box end caps (dark): front-top -> splayed back-top -> plane BACK edge -> plane FRONT edge.
    // The bottom edge is the diagonal that bridges the plane's back and front.
    quad(CAP, cx - hw, frontTop, cx - hw - capW, topBack, cx - hw - capW, planeTop, cx - hw, planeBot);
    quad(CAP, cx + hw, frontTop, cx + hw + capW, topBack, cx + hw + capW, planeTop, cx + hw, planeBot);
    // Front face (grey): sits at the plane's front edge.
    quad(GREY, cx - hw, frontTop, cx + hw, frontTop, cx + hw, planeBot, cx - hw, planeBot);
    // Top face (white trapezoid: front edge -> splayed, raised back edge).
    quad(WHITE, cx - hw, frontTop, cx + hw, frontTop, cx + hw + capW, topBack, cx - hw - capW, topBack);

    return img;
}

juce::Image bakeGridline(float thickness, juce::Colour colour, float arch)
{
    // A thin flat bar spanning the fretboard: full-width top edge, diagonal-cut ends, bowed by
    // the parabola arc(x) = arch * t^2 (t = -1..1 across the content). The centre sits at the
    // top of the bow and the ends drop by `arch`, matching the notes' centre-lifted highway
    // curve. Margins match the old marker PNGs (89px) so the drawn width lines up unchanged.
    const int   W        = kGridlineWidth;
    const float x0       = 89.0f;
    const float x1       = (float) W - 89.0f;
    const float cx       = (x0 + x1) * 0.5f;
    const float half     = (x1 - x0) * 0.5f;
    const float endInset = 40.0f;   // horizontal inset of the bottom corners -> diagonal ends
    const float topPad   = 12.0f;   // headroom above the centre so the bar never clips row 0

    auto arc = [&](float x) { float t = (x - cx) / half; return arch * t * t; };

    const int H = (int) std::ceil(topPad + thickness + std::abs(arch) + 12.0f);
    juce::Image img(juce::Image::ARGB, W, H, true);
    juce::Graphics g(img);

    // Tessellate the bowed edges so the parabola is smooth (a 4-corner quad would give straight
    // edges between the ends). Top edge L->R at full width; bottom edge R->L inset at both ends.
    constexpr int N = 256;
    juce::Path p;
    for (int i = 0; i <= N; ++i)
    {
        float x = x0 + (x1 - x0) * (float) i / (float) N;
        float y = topPad + arc(x);
        if (i == 0) p.startNewSubPath(x, y);
        else        p.lineTo(x, y);
    }
    const float bx0 = x0 + endInset, bx1 = x1 - endInset;
    for (int i = 0; i <= N; ++i)
    {
        float x = bx1 - (bx1 - bx0) * (float) i / (float) N;
        float y = topPad + thickness + arc(x);
        p.lineTo(x, y);
    }
    p.closeSubPath();

    g.setColour(colour);
    g.fillPath(p);
    return img;
}

juce::Image bakeBar(const BarRamp& ramp,
                    juce::Rectangle<int> canvas,
                    juce::Rectangle<int> contentBounds,
                    float thickness,
                    float centreLineAlpha)
{
    juce::Image img(juce::Image::ARGB, canvas.getWidth(), canvas.getHeight(), true);

    // --- Tube: per-pixel, arc-shifted vertical ramp ---
    {
        juce::Image::BitmapData bd(img, juce::Image::BitmapData::writeOnly);
        float sx = contentBounds.getWidth() / kSrcW;
        float sy = contentBounds.getHeight() / kSrcH;

        for (int px = 0; px < canvas.getWidth(); ++px)
        {
            float x = (px - contentBounds.getX()) / sx;    // source units
            if (x < kTubeX0 || x > kSrcW - kTubeX0) continue;

            float top = arcTopAt(x);
            const float syEff = sy * thickness;
            for (int py = 0; py < canvas.getHeight(); ++py)
            {
                // Pixel's span in source units, for edge coverage AA
                float y0 = unsqueezeY(x, (py - contentBounds.getY()) / sy, thickness);
                float y1 = y0 + 1.0f / syEff;
                float overlap = juce::jmin(y1, top + kTubeH) - juce::jmax(y0, top);
                if (overlap <= 0.0f) continue;

                float t = juce::jlimit(0.0f, 1.0f, ((y0 + y1) * 0.5f - top) / kTubeH);
                float cov = juce::jmin(1.0f, overlap * syEff);
                auto col = rampAt(ramp, t);
                bd.setPixelColour(px, py, cov >= 1.0f ? col : col.withAlpha(cov));
            }
        }
    }

    // --- Shadow sliver on the tube along each cap's inner edge, then the
    // caps themselves (per-pixel arc shading) on top. Graphics is scoped so
    // its renderer is gone before shadeCap touches the bitmap directly. ---
    {
        juce::Graphics g(img);
        auto toContent = juce::AffineTransform::scale(contentBounds.getWidth() / kSrcW,
                                                      contentBounds.getHeight() / kSrcH)
                             .translated((float) contentBounds.getX(),
                                         (float) contentBounds.getY());
        g.addTransform(toContent);

        juce::Path sliver;
        sliver.addQuadrilateral(82.0f, 27.0f, 90.0f, 27.0f, 56.0f, 152.0f, 48.0f, 152.0f);
        // Squeeze about the tube centreline at the cap (mirrored x, so the same value serves
        // both ends), matching the tube and cap.
        const float capCy = arcTopAt(65.0f) + kTubeH * 0.5f;
        sliver.applyTransform(juce::AffineTransform::translation(0.0f, -capCy)
                                  .scaled(1.0f, thickness)
                                  .translated(0.0f, capCy));
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillPath(sliver);
        sliver.applyTransform(juce::AffineTransform::scale(-1.0f, 1.0f).translated(kSrcW, 0.0f));
        g.fillPath(sliver);
    }

    shadeCap(img, contentBounds, true, thickness);
    shadeCap(img, contentBounds, false, thickness);

    // Accent line: a bright stripe along the tube's own centreline, so it follows the arc
    // rather than cutting straight across it. Drawn last, over the caps.
    if (centreLineAlpha > 0.0f)
    {
        juce::Image::BitmapData bd(img, juce::Image::BitmapData::readWrite);
        const float sx = contentBounds.getWidth() / kSrcW;
        const float sy = contentBounds.getHeight() / kSrcH;
        const float halfBand = kTubeH * kAccentLineHeight * 0.5f;

        for (int px = 0; px < canvas.getWidth(); ++px)
        {
            float x = (px - contentBounds.getX()) / sx;
            // Stop short of the end caps. They lean, so a horizontal band through them
            // flares into a bowtie instead of reading as a line down the tube.
            if (x < kAccentLineInset || x > kSrcW - kAccentLineInset) continue;
            const float cy = arcTopAt(x) + kTubeH * 0.5f;

            for (int py = 0; py < canvas.getHeight(); ++py)
            {
                float y = unsqueezeY(x, (py - contentBounds.getY()) / sy, thickness);
                if (std::abs(y - cy) > halfBand) continue;
                auto base = bd.getPixelColour(px, py);
                if (base.getAlpha() == 0) continue;   // never widen the bar
                bd.setPixelColour(px, py,
                    base.interpolatedWith(juce::Colours::white, centreLineAlpha));
            }
        }
    }

    return img;
}

} // namespace GemArt
