/*
    ==============================================================================

        ProceduralTrackArt.cpp
        Author: Noah Baxter

    ==============================================================================
*/

#include "ProceduralTrackArt.h"

namespace ProceduralTrackArt
{
    // Strikeline art colours, all named in one place so they're easy to tune. The
    // per-lane pad colours live in TrackRenderer's strikePadColours table.
    namespace StrikeArt
    {
        // Toggle: draw the dark recessed frame behind the pads. Off = transparent, so
        // the pads/connectors float straight on the highway. Flip to true to bring the
        // dark strike-zone tray back.
        constexpr bool drawBand = false;

        const juce::Colour band           { 0xFF0C0C0C };   // dark strike-zone strip behind the pads
        const juce::Colour connectorBack  { 0xFF080808 };   // chrome connector dark backing
        const juce::Colour chromeEdge     { 0xFF8A8A8A };   // silver bar edge (dark side of the highlight)
        const juce::Colour chromeCentre   { 0xFFF2F2F2 };   // silver bar bright centre
        const juce::Colour groove         { 0xFF141414 };   // dark groove between the double bars
        constexpr float interiorTopMul    = 0.28f;          // pad interior brightness vs border (top)
        constexpr float interiorBottomMul = 0.30f;          // pad interior brightness vs border (bottom)
    }

    void drawSidebarRails(juce::Graphics& g,
                          const std::vector<EdgeStrip>& edges, int stripCount,
                          float railFrac)
    {
        if (stripCount < 1 || (int) edges.size() <= stripCount) return;
        const int n = stripCount;

        // Cross-section from the board edge outward, pixel-matched to sidebars.png:
        // dark-grey inside edge | grey line | black track | thin grey line. Widths are
        // relative units; the rail's total width = railFrac of the LOCAL board width,
        // so the rail foreshortens (thins) down the neck with the board.
        struct Band { float width; juce::Colour colour; };
        const Band bands[] = {
            { RailGeom::bandUnits[0], juce::Colour(0xFF404040) },  // dark-grey inside edge
            { RailGeom::bandUnits[1], juce::Colour(0xFF606060) },  // grey line (3rd from outer)
            { RailGeom::bandUnits[2], juce::Colour(0xFF060606) },  // black track
            { RailGeom::bandUnits[3], juce::Colour(0xFF606060) },  // thin grey line (outer)
        };
        const float total = RailGeom::totalUnits;

        const float inset = RailGeom::insetUnits;   // units; shifts the whole rail inward to line up with the strikeline

        // Fill a ribbon between two unit-offsets from the board edge (0 = on the
        // edge, increasing = outward). rightSide mirrors to the right rail.
        auto fillBand = [&](bool rightSide, float offInner, float offOuter, juce::Colour c)
        {
            auto xAt = [&](int i, float off)
            {
                const auto& e = edges[i].first;
                float unit = railFrac * (e.rightX - e.leftX) / total;
                return rightSide ? e.rightX + off * unit : e.leftX - off * unit;
            };
            juce::Path band;
            band.startNewSubPath(xAt(0, offInner), edges[0].first.centerY);
            for (int i = 1; i <= n; i++) band.lineTo(xAt(i, offInner), edges[i].first.centerY);
            for (int i = n; i >= 0; i--) band.lineTo(xAt(i, offOuter), edges[i].first.centerY);
            band.closeSubPath();
            g.setColour(c);
            g.fillPath(band);
        };

        for (bool right : { false, true })
        {
            float off = -inset;
            for (const auto& b : bands)
            {
                fillBand(right, off, off + b.width, b.colour);
                off += b.width;
            }
        }
    }

    void drawStrikelinePads(juce::Graphics& g,
                            const std::vector<StrikePad>& pads,
                            bool endCaps, float barHalfFrac)
    {
        auto quad = [](juce::Point<float> a, juce::Point<float> b,
                       juce::Point<float> c, juce::Point<float> d)
        {
            juce::Path p;
            p.startNewSubPath(a); p.lineTo(b); p.lineTo(c); p.lineTo(d); p.closeSubPath();
            return p;
        };
        auto lerp = [](juce::Point<float> p, juce::Point<float> q, float t)
        { return juce::Point<float>(p.x + (q.x - p.x) * t, p.y + (q.y - p.y) * t); };
        auto unit = [](juce::Point<float> v)
        {
            float L = std::sqrt(v.x * v.x + v.y * v.y);
            return L > 1e-4f ? juce::Point<float>(v.x / L, v.y / L) : juce::Point<float>();
        };

        // Dark strike-zone frame hugging the pad row: top and bottom follow the
        // pads' arc (small margin) and it reaches the end caps horizontally, so the
        // opaque mask matches the reference instead of a flat-topped block. Gated by
        // StrikeArt::drawBand (off by default -> transparent).
        if (StrikeArt::drawBand && ! pads.empty())
        {
            const float mTop = 1.0f, mBot = 8.0f;   // dark margin above / below the pads
            const auto& f = pads.front();
            const auto& l = pads.back();
            // Horizontal frame margin past the outer pads. With end caps it is wide
            // enough to house the silver bar; without, just a thin dark border.
            float capW = (f.nearR.x - f.nearL.x) * (endCaps ? 0.14f : 0.05f);
            auto dy = [](juce::Point<float> p, float d) { return juce::Point<float>(p.x, p.y + d); };
            juce::Path bandPath;
            bandPath.startNewSubPath(dy({ f.farL.x - capW, f.farL.y }, -mTop));
            for (const auto& p : pads) { bandPath.lineTo(dy(p.farL, -mTop)); bandPath.lineTo(dy(p.farR, -mTop)); }
            bandPath.lineTo(dy({ l.farR.x + capW, l.farR.y }, -mTop));
            bandPath.lineTo(dy({ l.nearR.x + capW, l.nearR.y }, mBot));
            for (auto it = pads.rbegin(); it != pads.rend(); ++it) { bandPath.lineTo(dy(it->nearR, mBot)); bandPath.lineTo(dy(it->nearL, mBot)); }
            bandPath.lineTo(dy({ f.nearL.x - capW, f.nearL.y }, mBot));
            bandPath.closeSubPath();
            g.setColour(StrikeArt::band);
            g.fillPath(bandPath);
        }

        // Chrome connector: dark backing + two crisp (angled) silver bars split by a
        // dark groove. Only the colour is a gradient, bright at each bar's centre and
        // darker toward its edges; the bar shapes stay straight-edged.
        // One silver bar filled with a gradient bright band across its centre,
        // perpendicular to the (possibly angled) bar.
        auto silverBar = [&](juce::Point<float> na, juce::Point<float> nb,
                             juce::Point<float> fa, juce::Point<float> fb)
        {
            juce::Point<float> A = (na + nb) * 0.5f;   // near centre
            juce::Point<float> B = (fa + fb) * 0.5f;   // far centre
            juce::ColourGradient grad(StrikeArt::chromeEdge, A.x, A.y,
                                      StrikeArt::chromeEdge, B.x, B.y, false);
            grad.addColour(0.5, StrikeArt::chromeCentre);
            g.setGradientFill(grad);
            g.fillPath(quad(na, nb, fb, fa));
        };

        // Chrome connector: dark backing + silver bar(s). Between colours it is a
        // double bar split by a groove; the end caps are a single bar.
        auto chromeBar = [&](juce::Point<float> nO, juce::Point<float> nI,
                             juce::Point<float> fO, juce::Point<float> fI, bool doubleBar,
                             juce::Point<float> gN = {}, juce::Point<float> gF = {})
        {
            // The silver bar reaches all the way to the band bottom (near) and up to
            // the fret-top arc (far). Extend the near end down past the pad corner into
            // the mBot margin; the far end sits just above the far corners so the rod
            // follows the top-of-fret curve with no dark notch between frets. gN/gF (the
            // lane-boundary groove points, on the laneline) extend along their own
            // direction, which keeps them on the laneline.
            const float nearExt = 8.0f, farInset = -1.0f;
            auto extend = [&](juce::Point<float>& n, juce::Point<float>& f)
            {
                auto d = unit(n - f);          // far -> near (toward the near/bottom edge)
                n = n + d * nearExt;
                f = f + d * farInset;
            };
            extend(nO, fO);
            extend(nI, fI);
            extend(gN, gF);

            // Dark backing behind the whole gap only when the strike-zone band is on;
            // with it off (transparent default) the only dark mark in the gap should be
            // the single groove line between the two bars, not a solid dark fill.
            if (StrikeArt::drawBand)
            {
                g.setColour(StrikeArt::connectorBack);
                g.fillPath(quad(nO, nI, fI, fO));
            }
            if (doubleBar)
            {
                // Two bars centred ON the groove (gN..gF = the lane boundary), each a
                // fixed fraction of the gap wide, so the connector is symmetric about
                // the laneline with a consistent margin to the pads on both sides.
                auto len = [](juce::Point<float> v) { return std::sqrt(v.x * v.x + v.y * v.y); };
                juce::Point<float> aN = unit(nI - nO), aF = unit(fI - fO);
                float bwN = barHalfFrac * len(nI - nO), bwF = barHalfFrac * len(fI - fO);
                juce::Point<float> nL = gN - aN * bwN, nR = gN + aN * bwN;
                juce::Point<float> fL = gF - aF * bwF, fR = gF + aF * bwF;
                silverBar(nL, gN, fL, gF);
                silverBar(gN, nR, gF, fR);
                g.setColour(StrikeArt::groove);
                g.drawLine({ gN, gF }, 2.0f);
            }
            else
            {
                const float s0 = 0.30f, s2 = 0.70f;
                juce::Point<float> nL = lerp(nO, nI, s0), nR = lerp(nO, nI, s2);
                juce::Point<float> fL = lerp(fO, fI, s0), fR = lerp(fO, fI, s2);
                silverBar(nL, nR, fL, fR);
            }
        };

        // Separators between adjacent pads: double bar.
        for (size_t i = 0; i + 1 < pads.size(); i++)
            chromeBar(pads[i].nearR, pads[i + 1].nearL, pads[i].farR, pads[i + 1].farL, true,
                      pads[i].sepNear, pads[i].sepFar);

        // End caps just outside the first and last pad: single bar (drums/elite only;
        // guitar-like references have no outer cap).
        if (endCaps && ! pads.empty())
        {
            const auto& f = pads.front();
            const auto& l = pads.back();
            float capW = (f.nearR.x - f.nearL.x) * 0.14f;
            chromeBar({ f.nearL.x - capW, f.nearL.y }, f.nearL, { f.farL.x - capW, f.farL.y }, f.farL, false);
            chromeBar(l.nearR, { l.nearR.x + capW, l.nearR.y }, l.farR, { l.farR.x + capW, l.farR.y }, false);
        }

        // Pads: coloured border (thicker on the bottom "ridge" to match the PNG) +
        // near-black interior + a split raised above centre (bottom cell taller).
        const float borderTop    = 3.0f;
        const float borderSide   = 4.0f;
        const float borderBottom = 7.0f;   // thick lower ridge (~2x the top)
        const float splitPx      = 4.0f;
        const float splitFrac    = 0.42f;  // split position from top of interior (bottom cell taller)
        // Closed fill from a top polyline (L->R) and a bottom polyline (L->R).
        auto ribbon = [](const std::vector<juce::Point<float>>& top,
                         const std::vector<juce::Point<float>>& bot)
        {
            juce::Path p;
            p.startNewSubPath(top.front());
            for (size_t i = 1; i < top.size(); ++i) p.lineTo(top[i]);
            for (size_t i = bot.size(); i-- > 0; ) p.lineTo(bot[i]);
            p.closeSubPath();
            return p;
        };

        // Same ribbon but with the 4 outer corners rounded (radius r). The top/bottom
        // stay curved (the sampled points); only the TL/TR/BR/BL vertices get filleted.
        // r is clamped to a fraction of each adjacent segment so narrow pads (whose top
        // samples sit close together) don't overshoot and distort.
        auto roundedRibbon = [&](const std::vector<juce::Point<float>>& top,
                                 const std::vector<juce::Point<float>>& bot, float r)
        {
            std::vector<juce::Point<float>> b;
            b.reserve(top.size() + bot.size());
            for (const auto& q : top) b.push_back(q);
            for (size_t i = bot.size(); i-- > 0; ) b.push_back(bot[i]);
            const size_t m = b.size();
            auto isCorner = [&](size_t i)
            { return i == 0 || i == top.size() - 1 || i == top.size() || i == m - 1; };
            auto len = [](juce::Point<float> v) { return std::sqrt(v.x * v.x + v.y * v.y); };

            juce::Path p;
            bool started = false;
            for (size_t i = 0; i < m; ++i)
            {
                juce::Point<float> C = b[i];
                if (isCorner(i))
                {
                    juce::Point<float> prev = b[(i + m - 1) % m], next = b[(i + 1) % m];
                    juce::Point<float> pa = C + unit(prev - C) * std::min(r, 0.4f * len(prev - C));
                    juce::Point<float> pb = C + unit(next - C) * std::min(r, 0.4f * len(next - C));
                    if (!started) { p.startNewSubPath(pa); started = true; } else p.lineTo(pa);
                    p.quadraticTo(C, pb);
                }
                else
                {
                    if (!started) { p.startNewSubPath(C); started = true; } else p.lineTo(C);
                }
            }
            p.closeSubPath();
            return p;
        };
        const float cornerR = 3.0f;

        for (const auto& p : pads)
        {
            const auto& top = p.farEdge;    // curved top edge, L->R
            const auto& bot = p.nearEdge;   // curved bottom edge, L->R
            if (top.size() < 2 || bot.size() != top.size()) continue;
            const size_t n = top.size();

            juce::Point<float> topMid = (top.front() + top.back()) * 0.5f;
            juce::Point<float> botMid = (bot.front() + bot.back()) * 0.5f;

            // Gradient axis perpendicular to the pad's top/bottom edges, so the dark->
            // bright shading follows the fret's tilt/curve (iso-colour lines stay
            // parallel to the edges) instead of running straight up. For the centre pad
            // this reduces to the plain vertical axis; only the angled side pads rotate.
            juce::Point<float> topDir = unit(top.back() - top.front());
            juce::Point<float> nrm(-topDir.y, topDir.x);
            juce::Point<float> gd = botMid - topMid;
            if (nrm.x * gd.x + nrm.y * gd.y < 0.0f) nrm = { -nrm.x, -nrm.y };
            juce::Point<float> gBot = topMid + nrm * (nrm.x * gd.x + nrm.y * gd.y);

            // Bright colour border = outer curved ribbon, vertical gradient. The PNG holds
            // the dull top colour most of the way, then brightens only at the thick bottom
            // ridge, so keep it flat until ~60% then ramp.
            juce::ColourGradient bord(p.baseColour, topMid.x, topMid.y,
                                      p.bottomColour, gBot.x, gBot.y, false);
            bord.addColour(0.6, p.baseColour);
            g.setGradientFill(bord);
            g.fillPath(roundedRibbon(top, bot, cornerR));

            // Interior edges: shift each column inward (top down, bottom up) then trim the
            // horizontal ends so the coloured border shows on all four sides.
            std::vector<juce::Point<float>> iTop(n), iBot(n);
            for (size_t i = 0; i < n; ++i)
            {
                juce::Point<float> down = unit(bot[i] - top[i]);
                iTop[i] = top[i] + down * borderTop;
                iBot[i] = bot[i] - down * borderBottom;
            }
            auto trimSides = [&](std::vector<juce::Point<float>>& e)
            {
                e.front() += unit(e[1]       - e.front()) * borderSide;
                e.back()  += unit(e[n - 2]   - e.back())  * borderSide;
            };
            trimSides(iTop); trimSides(iBot);

            juce::ColourGradient inr(p.baseColour.withMultipliedBrightness(StrikeArt::interiorTopMul), topMid.x, topMid.y,
                                     p.bottomColour.withMultipliedBrightness(StrikeArt::interiorBottomMul), gBot.x, gBot.y, false);
            g.setGradientFill(inr);
            g.fillPath(roundedRibbon(iTop, iBot, cornerR * 0.7f));

            // Split band: a thin ribbon at splitFrac between the interior edges, carrying
            // the same curve/tilt.
            std::vector<juce::Point<float>> sTop(n), sBot(n);
            for (size_t i = 0; i < n; ++i)
            {
                juce::Point<float> s  = iTop[i] + (iBot[i] - iTop[i]) * splitFrac;
                juce::Point<float> up = unit(iTop[i] - iBot[i]) * (splitPx * 0.5f);
                sTop[i] = s + up; sBot[i] = s - up;
            }
            g.setColour(p.baseColour);
            g.fillPath(ribbon(sTop, sBot));
        }
    }
}
