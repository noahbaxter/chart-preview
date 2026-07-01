/*
    ==============================================================================

        ProceduralTrackArt.cpp
        Author: Noah Baxter

    ==============================================================================
*/

#include "ProceduralTrackArt.h"

namespace ProceduralTrackArt
{
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
            { 1.0f, juce::Colour(0xFF404040) },  // dark-grey inside edge
            { 1.4f, juce::Colour(0xFF606060) },  // grey line (3rd from outer)
            { 2.0f, juce::Colour(0xFF060606) },  // black track
            { 1.0f, juce::Colour(0xFF606060) },  // thin grey line (outer)
        };
        float total = 0.0f;
        for (const auto& b : bands) total += b.width;

        const float inset = 1.3f;   // units; shifts the whole rail inward to line up with the strikeline

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
}
