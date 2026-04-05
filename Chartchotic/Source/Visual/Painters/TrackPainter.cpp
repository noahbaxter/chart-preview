/*
    ==============================================================================

        TrackPainter.cpp
        Author:  Noah Baxter

        Stateless track/highway background drawing. Extracted from TrackRenderer.

    ==============================================================================
*/

#include "TrackPainter.h"

using namespace PositionConstants;

namespace TrackPainter
{

void paintBackground(juce::Graphics& g, int viewportW, int viewportH,
                     Part activePart, float posEnd,
                     const juce::Image& fadedTrackImage, bool showTrack)
{
    if (PositionMath::bemaniMode)
    {
        if (showTrack)
        {
            bool isDrums = isDrumLike(activePart);
            auto edge = PositionMath::getFretboardEdge(isDrums, 0.0f, viewportW, viewportH,
                            HIGHWAY_POS_START, posEnd);
            g.setColour(juce::Colour(0xFF111111));
            g.fillRect(edge.leftX, 0.0f, edge.rightX - edge.leftX, (float)viewportH);
        }
        return;
    }

    if (!fadedTrackImage.isValid()) return;

    if (fadedTrackImage.getWidth() == viewportW && fadedTrackImage.getHeight() == viewportH)
        g.drawImageAt(fadedTrackImage, 0, 0);
    else
        g.drawImage(fadedTrackImage, 0, 0, viewportW, viewportH,
                    0, 0, fadedTrackImage.getWidth(), fadedTrackImage.getHeight());
}

void paintFromCache(juce::Graphics& g, const juce::Image& cachedFadedTrack,
                    int viewportW, int viewportH,
                    Part activePart, float posEnd, bool showTrack)
{
    if (PositionMath::bemaniMode)
    {
        paintBackground(g, viewportW, viewportH, activePart, posEnd, {}, showTrack);
        return;
    }
    if (!cachedFadedTrack.isValid()) return;
    if (cachedFadedTrack.getWidth() == viewportW && cachedFadedTrack.getHeight() == viewportH)
        g.drawImageAt(cachedFadedTrack, 0, 0);
    else
        g.drawImage(cachedFadedTrack, 0, 0, viewportW, viewportH,
                    0, 0, cachedFadedTrack.getWidth(), cachedFadedTrack.getHeight());
}

void paintBemaniOverlay(juce::Graphics& g, int viewportW, int viewportH,
                        Part activePart, float posEnd,
                        bool showLaneSeparators, bool showStrikeline)
{
    if (!PositionMath::bemaniMode) return;

    bool isDrums = isDrumLike(activePart);
    auto edge = PositionMath::getFretboardEdge(isDrums, 0.0f, viewportW, viewportH,
                    HIGHWAY_POS_START, posEnd);
    float leftX = edge.leftX;
    float rightX = edge.rightX;
    float fbWidth = rightX - leftX;
    float h = (float)viewportH;

    // Lane dividers
    if (showLaneSeparators)
    {
        int numCols = isDrums ? 4 : 5;
        float laneAlpha = bemaniConfig.laneOpacity;
        g.setColour(juce::Colours::white.withAlpha(laneAlpha));
        float divW = std::max(1.0f, bemaniConfig.laneDivW);
        for (int i = 1; i < numCols; i++)
        {
            float cx = leftX + ((float)i / (float)numCols) * fbWidth;
            g.fillRect(cx - divW * 0.5f, 0.0f, divW, h);
        }
    }

    // Strikeline — colored rounded squares per lane
    if (showStrikeline)
    {
        float strikeAlpha = bemaniConfig.strikelineOpacity;
        float strikeFrac = bemaniConfig.strikelinePos;
        int numCols = isDrums ? 4 : 5;
        float colW = fbWidth / (float)numCols;
        float strikeY = h * strikeFrac;
        float padH = colW * 0.55f;
        float padY = strikeY - padH * 0.5f;
        float inset = colW * 0.08f;
        float corner = colW * 0.15f;

        static const juce::Colour guitarCols[] = {
            juce::Colours::green, juce::Colours::red, juce::Colours::yellow,
            juce::Colours::blue, juce::Colours::orange
        };
        static const juce::Colour drumCols[] = {
            juce::Colours::red, juce::Colours::yellow, juce::Colours::blue, juce::Colours::green
        };
        const juce::Colour* cols = isDrums ? drumCols : guitarCols;

        for (int i = 0; i < numCols; i++)
        {
            float cx = leftX + ((float)i + 0.5f) / (float)numCols * fbWidth;
            float pw = colW - inset * 2.0f;
            auto padRect = juce::Rectangle<float>(cx - pw * 0.5f, padY, pw, padH);

            g.setColour(cols[i].withAlpha(strikeAlpha * 0.5f));
            g.fillRoundedRectangle(padRect, corner);

            float innerInset = pw * 0.15f;
            auto innerRect = padRect.reduced(innerInset, padH * 0.2f);
            g.setColour(cols[i].darker(0.6f).withAlpha(strikeAlpha * 0.7f));
            g.fillRoundedRectangle(innerRect, corner * 0.5f);

            g.setColour(cols[i].withAlpha(strikeAlpha * 0.8f));
            g.drawRoundedRectangle(padRect, corner, 1.0f);
        }

        g.setColour(juce::Colours::white.withAlpha(strikeAlpha * 0.3f));
        g.drawHorizontalLine((int)strikeY, leftX, rightX);
    }
}

void paintBemaniSidebars(juce::Graphics& g, int viewportW, int viewportH,
                         Part activePart, float posEnd)
{
    if (!PositionMath::bemaniMode) return;

    bool isDrums = isDrumLike(activePart);
    auto edge = PositionMath::getFretboardEdge(isDrums, 0.0f, viewportW, viewportH,
                    HIGHWAY_POS_START, posEnd);
    float leftX = edge.leftX;
    float rightX = edge.rightX;
    float h = (float)viewportH;
    float viewW = (float)viewportW;

    float pad = (rightX - leftX) * 0.06f;
    float maskL = leftX - pad;
    float maskR = rightX + pad;

    g.setColour(juce::Colours::black);
    g.fillRect(-viewW, -h, maskL + viewW, h * 3.0f);
    g.fillRect(maskR, -h, viewW * 2.0f, h * 3.0f);
}

void paintBemaniRails(juce::Graphics& g, int viewportW, int viewportH,
                      Part activePart, float posEnd)
{
    if (!PositionMath::bemaniMode) return;

    bool isDrums = isDrumLike(activePart);
    auto edge = PositionMath::getFretboardEdge(isDrums, 0.0f, viewportW, viewportH,
                    HIGHWAY_POS_START, posEnd);
    float leftX = edge.leftX;
    float rightX = edge.rightX;
    float h = (float)viewportH;

    float fbW = rightX - leftX;
    float railInset = bemaniConfig.railInset * fbW;
    float outerGreyW = std::max(1.0f, fbW * 0.004f);
    float blackW     = std::max(2.0f, fbW * 0.012f);
    float innerGreyW = std::max(1.5f, fbW * 0.006f);
    float totalRailW = outerGreyW + blackW + innerGreyW;

    auto innerGrey = juce::Colour(0xff8c8c8c);
    auto outerGrey = juce::Colour(0xff606060);
    auto darkCol   = juce::Colour(0xff1a1a1a);

    // Left rail
    {
        float x = leftX + railInset - totalRailW + innerGreyW;
        g.setColour(outerGrey);
        g.fillRect(x, 0.0f, outerGreyW, h);
        x += outerGreyW;
        g.setColour(darkCol);
        g.fillRect(x, 0.0f, blackW, h);
        x += blackW;
        g.setColour(innerGrey);
        g.fillRect(x, 0.0f, innerGreyW, h);
    }

    // Right rail
    {
        float x = rightX - railInset - innerGreyW;
        g.setColour(innerGrey);
        g.fillRect(x, 0.0f, innerGreyW, h);
        x += innerGreyW;
        g.setColour(darkCol);
        g.fillRect(x, 0.0f, blackW, h);
        x += blackW;
        g.setColour(outerGrey);
        g.fillRect(x, 0.0f, outerGreyW, h);
    }
}

void paintBemaniTexture(juce::Graphics& g, const juce::Image& sourceTexture,
                        float scrollOffset, float textureScale, float textureOpacity,
                        int targetW, int targetH,
                        Part activePart, float posEnd)
{
    if (!PositionMath::bemaniMode) return;
    if (!sourceTexture.isValid()) return;

    bool isDrums = isDrumLike(activePart);
    auto edge = PositionMath::getFretboardEdge(isDrums, 0.0f, targetW, targetH,
                    HIGHWAY_POS_START, posEnd);
    float leftX = edge.leftX;
    float hwyW = edge.rightX - edge.leftX;
    if (hwyW <= 0) return;

    float texAspect = (float)sourceTexture.getWidth() / (float)sourceTexture.getHeight();
    float tileW = hwyW;
    float tileH = tileW / texAspect * textureScale;
    if (tileH < 1.0f) return;

    float strikeFrac = bemaniConfig.strikelinePos;
    float texSpeed = bemaniConfig.texSpeed;
    float hwyScale = std::max(0.1f, PositionMath::bemaniHwyScale);
    float totalPx = scrollOffset * strikeFrac * PositionConstants::REFERENCE_HEIGHT * texSpeed / hwyScale;
    float offset = std::fmod(totalPx, tileH);
    if (offset < 0.0f) offset += tileH;

    g.saveState();
    g.reduceClipRegion((int)leftX, 0, (int)std::ceil(hwyW), targetH);
    g.setOpacity(textureOpacity);
    float startY = (float)targetH - offset;
    for (float y = startY; y > -tileH; y -= tileH)
        g.drawImage(sourceTexture, leftX, y, tileW, tileH,
                    0, 0, sourceTexture.getWidth(), sourceTexture.getHeight());
    g.restoreState();
}

} // namespace TrackPainter
