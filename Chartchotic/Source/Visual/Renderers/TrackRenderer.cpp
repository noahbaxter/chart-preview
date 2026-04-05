/*
    ==============================================================================

        TrackRenderer.cpp
        Author:  Noah Baxter

    ==============================================================================
*/

#include "TrackRenderer.h"

using namespace PositionConstants;

TrackRenderer::TrackRenderer(juce::ValueTree& state)
    : state(state)
{
#ifndef CHARTCHOTIC_NO_BINARY_DATA
    // Load layer images from BinaryData
    sidebarsImage = juce::ImageCache::getFromMemory(BinaryData::sidebars_png, BinaryData::sidebars_pngSize);
    strikelineGuitarImage = juce::ImageCache::getFromMemory(BinaryData::strikeline_guitar_png, BinaryData::strikeline_guitar_pngSize);
    strikelineDrumsImage = juce::ImageCache::getFromMemory(BinaryData::strikeline_drums_png, BinaryData::strikeline_drums_pngSize);
    strikelineConnectorsImage = juce::ImageCache::getFromMemory(BinaryData::strikeline_connectors_png, BinaryData::strikeline_connectors_pngSize);
    kickSmashersImage = juce::ImageCache::getFromMemory(BinaryData::kick_smashers_png, BinaryData::kick_smashers_pngSize);
#endif
}

void TrackRenderer::paint(juce::Graphics& g, int viewportWidth, int viewportHeight)
{
    bool showTrack = !state.hasProperty("showTrack") || (bool)state["showTrack"];
    TrackPainter::paintBackground(g, viewportWidth, viewportHeight,
                                  activePart, cached.posEnd,
                                  fadedTrackImage, showTrack);
}

void TrackRenderer::paintBemaniOverlay(juce::Graphics& g, int viewportWidth, int viewportHeight)
{
    bool showLaneSeps = !state.hasProperty("showLaneSeparators") || (bool)state["showLaneSeparators"];
    bool showStrike = !state.hasProperty("showStrikeline") || (bool)state["showStrikeline"];
    TrackPainter::paintBemaniOverlay(g, viewportWidth, viewportHeight,
                                     activePart, cached.posEnd,
                                     showLaneSeps, showStrike);
}

void TrackRenderer::paintBemaniSidebars(juce::Graphics& g, int viewportWidth, int viewportHeight)
{
    TrackPainter::paintBemaniSidebars(g, viewportWidth, viewportHeight,
                                      activePart, cached.posEnd);
}

void TrackRenderer::paintBemaniRails(juce::Graphics& g, int viewportWidth, int viewportHeight)
{
    TrackPainter::paintBemaniRails(g, viewportWidth, viewportHeight,
                                   activePart, cached.posEnd);
}

void TrackRenderer::paintTexture(juce::Graphics& g, float scrollOffset, int targetW, int targetH)
{
    if (PositionMath::bemaniMode)
    {
        if (!textureEnabled) return;
        TrackPainter::paintBemaniTexture(g, sourceTexture, scrollOffset, textureScale, textureOpacity,
                                         targetW, targetH, activePart, cached.posEnd);
        return;
    }
#ifdef DEBUG
    if (PositionMath::debugPolyShade) return;
#endif
    if (!textureEnabled || !prebaked.valid)
        return;

    int w = cached.width;
    int h = cached.totalHeight();
    int tileH = prebaked.tileHeight;

    // Ensure offscreen buffer
    if (offscreen.getWidth() != w || offscreen.getHeight() != h)
        offscreen = juce::Image(juce::Image::ARGB, w, h, true);
    else
        offscreen.clear({0, 0, w, h});

    {
        juce::Image::BitmapData dst(offscreen, juce::Image::BitmapData::writeOnly);
        juce::Image::BitmapData src(prebaked.mipAtlas, juce::Image::BitmapData::readOnly);

        for (int y = prebaked.yMin; y <= prebaked.yMax; y++)
        {
            auto& sl = prebaked.scanlines[y];
            if (sl.alpha <= 0.0f || sl.rightX <= sl.leftX) continue;

            int span = sl.rightX - sl.leftX;

            // Pick finest mip level that covers the span (never downsample > 2:1)
            int mipIdx = 0;
            for (int m = 1; m < (int)prebaked.mips.size(); m++)
            {
                if (prebaked.mips[m].width >= span)
                    mipIdx = m;
                else
                    break;
            }
            auto& mip = prebaked.mips[mipIdx];

            float texV = std::fmod((sl.texV + scrollOffset) * textureScale, 1.0f);
            if (texV < 0.0f) texV += 1.0f;

            int srcRow = mip.rowOffset + (int)(texV * tileH) % tileH;
            uint8_t alphaScale = (uint8_t)(sl.alpha * 255.0f);

            auto* srcLine = (juce::PixelARGB*)src.getLinePointer(srcRow);
            auto* dstLine = (juce::PixelARGB*)dst.getLinePointer(y);

            float invSpan = 1.0f / (float)span;

            for (int x = sl.leftX; x <= sl.rightX; x++)
            {
                int srcX = (int)((float)(x - sl.leftX) * invSpan * (float)(mip.width - 1));
                juce::PixelARGB px = srcLine[srcX];
                px.multiplyAlpha(alphaScale);
                dstLine[x] = px;
            }
        }
    } // BitmapData destroyed here — flushes CPU writes to GPU on Windows

    // Composite — stretch if baked size doesn't match virtual scene (during resize)
    g.setOpacity(textureOpacity);
    if (offscreen.getWidth() == targetW && offscreen.getHeight() == targetH)
        g.drawImageAt(offscreen, 0, 0);
    else
        g.drawImage(offscreen, 0, 0, targetW, targetH,
                    0, 0, offscreen.getWidth(), offscreen.getHeight());
}

void TrackRenderer::compositeLayers(juce::Image& target, int w, int h, bool isDrums,
                                     float posEnd, float farFadeEnd)
{
    juce::Graphics g(target);

    // Fill fretboard polygon from cached edges (shared with texture scanline LUT)
    juce::Path fretboardPath;
    int n = cached.stripCount;

    fretboardPath.startNewSubPath(cached.edges[0].first.rightX, cached.edges[0].first.centerY);
    for (int i = 1; i <= n; i++)
        fretboardPath.lineTo(cached.edges[i].first.rightX, cached.edges[i].first.centerY);
    for (int i = n; i >= 0; i--)
        fretboardPath.lineTo(cached.edges[i].first.leftX, cached.edges[i].first.centerY);
    fretboardPath.closeSubPath();

#ifdef DEBUG
    g.setColour(PositionMath::debugPolyShade ? juce::Colour(0xFFFF0000) : juce::Colour(0xFF111111));
#else
    g.setColour(juce::Colour(0xFF111111));
#endif
    g.fillPath(fretboardPath);
}

void TrackRenderer::bakeLayerImage(juce::Image& out, const juce::Image& src, const LayerTransform& t,
                                    int w, int h, int overflow, bool isDrums, bool tiled,
                                    float farFadeEnd, float farFadeLen, float farFadeCurve,
                                    float posEnd)
{
    if (!src.isValid()) { out = {}; return; }

    out = juce::Image(juce::Image::ARGB, w, h, true);
    {
        juce::Graphics g(out);
        float imgAspect = (float)src.getWidth() / (float)src.getHeight();
        int origH = h - overflow;   // yOffset is tuned relative to viewport height
        float drawW = (float)w * t.scale;
        float drawH = drawW / imgAspect;
        float drawX = ((float)w - drawW) * 0.5f + t.xOffset * (float)w;
        float drawY = (float)h - drawH + t.yOffset * (float)origH;

        if (tiled)
        {
            float tileBottom = drawY + drawH;
            float scale = 1.0f;
            while (tileBottom > 0)
            {
                float tileW = drawW * scale;
                float tileH = drawH * scale;
                float tileX = ((float)w - tileW) * 0.5f + t.xOffset * (float)w;
                float tileY = tileBottom - tileH;

                g.drawImage(src, {tileX, tileY, tileW, tileH});
                tileBottom -= tileH * tileStep;
                if (tileH < 1.0f) break;
                scale *= tileScaleStep;
            }
        }
        else
        {
            g.drawImage(src, {drawX, drawY, drawW, drawH});
        }
    }

    applyFarFade(out, w, h, overflow, isDrums, farFadeEnd, farFadeLen, farFadeCurve,
                 posEnd);
}

void TrackRenderer::rebuild(int width, int height, int overflow,
                               float farFadeEnd, float farFadeLen, float farFadeCurve,
                               float posEnd, bool geometryOnly)
{
    if (width <= 0 || height <= 0) return;

    // Skip if nothing changed
    if (width == cached.width && height == cached.height && overflow == cached.overflow &&
        posEnd == cached.posEnd && farFadeEnd == cached.fadeEnd &&
        farFadeLen == cached.fadeLen && farFadeCurve == cached.fadeCurve &&
        (isDrumLike(activePart)) == cached.isDrums)
        return;

    bool isDrums = isDrumLike(activePart);
    int totalH = height + overflow;

    if (PositionMath::bemaniMode)
    {
        // In Bemani mode, skip all perspective baking — draw flat programmatically in paint()
        cached.width = width;
        cached.height = height;
        cached.overflow = overflow;
        cached.isDrums = isDrums;
        cached.posEnd = posEnd;
        cached.fadeEnd = farFadeEnd;
        cached.fadeLen = farFadeLen;
        cached.fadeCurve = farFadeCurve;

        fadedTrackImage = {};
        for (auto& img : layerImages) img = {};
        prebaked.valid = false;
        return;
    }

    // Rebuild cached edge geometry (shared by polygon fill and texture scanline LUT)
    // Perspective math uses original viewport height; overflow offsets Y into the taller bitmap.
    float effectiveEnd = std::max(posEnd, farFadeEnd);
    float posRange = effectiveEnd - HIGHWAY_POS_START;

    auto edgeNear = PositionMath::getFretboardEdge(isDrums, HIGHWAY_POS_START, width, height,
                                                    HIGHWAY_POS_START, posEnd);
    auto edgeFar = PositionMath::getFretboardEdge(isDrums, effectiveEnd, width, height,
                                                   HIGHWAY_POS_START, posEnd);
    int pixelHeight = std::max(1, (int)(edgeNear.centerY - edgeFar.centerY));
    cached.stripCount = std::clamp(pixelHeight / PIXELS_PER_STRIP, MIN_STRIPS, MAX_STRIPS);

    cached.edges.resize(cached.stripCount + 1);
    for (int i = 0; i <= cached.stripCount; i++)
    {
        float pos = HIGHWAY_POS_START + posRange * (float)i / (float)cached.stripCount;
        auto edge = PositionMath::getFretboardEdge(isDrums, pos, width, height,
                                                    HIGHWAY_POS_START, posEnd);
        edge.centerY += (float)overflow;  // offset into taller bitmap
        cached.edges[i] = { edge, pos };
    }

    cached.width = width;
    cached.height = height;
    cached.overflow = overflow;
    cached.isDrums = isDrums;
    cached.posEnd = posEnd;
    cached.fadeEnd = farFadeEnd;
    cached.fadeLen = farFadeLen;
    cached.fadeCurve = farFadeCurve;

    if (!geometryOnly)
    {
        // Bake dark fill base (uses cached edges for polygon — already offset by overflow)
        fadedTrackImage = juce::Image(juce::Image::ARGB, width, totalH, true);
        compositeLayers(fadedTrackImage, width, totalH, isDrums, posEnd, farFadeEnd);
#ifdef DEBUG
        if (!PositionMath::debugPolyShade)
#endif
        applyFarFade(fadedTrackImage, width, totalH, overflow, isDrums, farFadeEnd, farFadeLen, farFadeCurve,
                     posEnd);

        // Bake individual overlay layers (drawn at interleaved z-positions by SceneRenderer)
        auto* layers = isDrums ? layersDrums : layersGuitar;
        bakeLayerImage(layerImages[SIDEBARS], sidebarsImage, layers[SIDEBARS],
                       width, totalH, overflow, isDrums, true, farFadeEnd, farFadeLen, farFadeCurve, posEnd);
        bakeLaneLinesPerspective(width, totalH, overflow, isDrums,
                                  farFadeEnd, farFadeLen, farFadeCurve, posEnd);
        bakeLayerImage(layerImages[STRIKELINE], isDrums ? strikelineDrumsImage : strikelineGuitarImage, layers[STRIKELINE],
                       width, totalH, overflow, isDrums, false, farFadeEnd, farFadeLen, farFadeCurve, posEnd);
        bakeLayerImage(layerImages[CONNECTORS], isDrums ? kickSmashersImage : strikelineConnectorsImage, layers[CONNECTORS],
                       width, totalH, overflow, isDrums, false, farFadeEnd, farFadeLen, farFadeCurve, posEnd);
    }

    rebuildPrebake();
}

void TrackRenderer::bakeLaneLinesPerspective(int w, int h, int overflow, bool isDrums,
                                               float farFadeEnd, float farFadeLen, float farFadeCurve,
                                               float posEnd)
{
    auto& out = layerImages[LANE_LINES];

    if (!laneCoords_ || laneCount_ < 2) {
        out = {};
        return;
    }

    out = juce::Image(juce::Image::ARGB, w, h, true);

    const auto& fbCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;

    // Compute boundary fractions between adjacent inner lanes (skip bar lane 0)
    std::vector<float> boundaryFracs;
    for (int i = 1; i < laneCount_ - 1; i++)
    {
        float rightNorm = laneCoords_[i].normX1 + laneCoords_[i].normWidth1;
        float leftNorm = laneCoords_[i + 1].normX1;
        float midNorm = (rightNorm + leftNorm) * 0.5f;
        boundaryFracs.push_back((midNorm - fbCoords.normX1) / fbCoords.normWidth1);
    }

    if (boundaryFracs.empty()) { out = {}; return; }

    {
        juce::Graphics g(out);
        g.setColour(juce::Colour(0x40FFFFFF));

        for (float frac : boundaryFracs)
        {
            juce::Path path;
            bool started = false;

            for (int i = 0; i <= cached.stripCount; i++)
            {
                auto& [edge, pos] = cached.edges[i];
                float edgeCenter = (edge.leftX + edge.rightX) * 0.5f;
                float edgeWidth = (edge.rightX - edge.leftX) * FRETBOARD_SCALE;
                float scaledLeft = edgeCenter - edgeWidth * 0.5f;
                float x = scaledLeft + frac * edgeWidth;

                if (!started) {
                    path.startNewSubPath(x, edge.centerY);
                    started = true;
                } else {
                    path.lineTo(x, edge.centerY);
                }
            }

            g.strokePath(path, juce::PathStrokeType(1.5f));
        }
    }

    applyFarFade(out, w, h, overflow, isDrums, farFadeEnd, farFadeLen, farFadeCurve,
                 posEnd);
}

void TrackRenderer::rebuildPrebake()
{
    if (cached.stripCount == 0 || cached.width <= 0 || cached.height <= 0 || !sourceTexture.isValid())
    {
        prebaked.valid = false;
        return;
    }

    int w = cached.width;
    int h = cached.totalHeight();   // use total (viewport + overflow) for scanline buffer
    int texH = sourceTexture.getHeight();

    // 1. Build per-scanline LUT by interpolating between cached edge pairs
    // Edges are already offset by overflow, so scanlines cover the full bitmap.
    prebaked.scanlines.assign(h, {0.0f, 0.0f, 0, 0});
    prebaked.yMin = h;
    prebaked.yMax = 0;

    for (int i = 0; i < cached.stripCount; i++)
    {
        auto& [nearC, nearPos] = cached.edges[i];      // nearer to strikeline (bottom, large Y)
        auto& [farC, farPos] = cached.edges[i + 1];    // farther from strikeline (top, small Y)

        int yStart = std::max(0, (int)std::ceil(farC.centerY));
        int yEnd = std::min(h - 1, (int)std::floor(nearC.centerY));

        float yRange = nearC.centerY - farC.centerY;

        for (int y = yStart; y <= yEnd; y++)
        {
            float t = (yRange > 0.0f) ? (nearC.centerY - (float)y) / yRange : 0.0f;
            t = juce::jlimit(0.0f, 1.0f, t);

            float pos = nearPos + t * (farPos - nearPos);
            float lx = nearC.leftX + t * (farC.leftX - nearC.leftX);
            float rx = nearC.rightX + t * (farC.rightX - nearC.rightX);

            prebaked.scanlines[y].texV = 1.0f - pos;
            prebaked.scanlines[y].alpha = calculateFarFade(pos, cached.fadeEnd, cached.fadeLen, cached.fadeCurve);
            prebaked.scanlines[y].leftX = std::max(0, (int)lx);
            prebaked.scanlines[y].rightX = std::min(w - 1, (int)rx);

            prebaked.yMin = std::min(prebaked.yMin, y);
            prebaked.yMax = std::max(prebaked.yMax, y);
        }
    }

    // 2. Pre-bake mip atlas: source texture at multiple horizontal widths (halving each level)
    prebaked.tileHeight = texH * PREBAKE_QUALITY;
    prebaked.mips.clear();

    int mipW = w;
    int rowOffset = 0;
    while (mipW >= MIN_MIP_WIDTH)
    {
        prebaked.mips.push_back({rowOffset, mipW});
        rowOffset += prebaked.tileHeight;
        mipW /= 2;
    }

    prebaked.mipAtlas = juce::Image(juce::Image::ARGB, w, rowOffset, true);
    {
        juce::Graphics tg(prebaked.mipAtlas);
        for (auto& mip : prebaked.mips)
            tg.drawImage(sourceTexture, {0.0f, (float)mip.rowOffset, (float)mip.width, (float)prebaked.tileHeight});
    }

    prebaked.valid = true;
}

void TrackRenderer::setTexture(const juce::Image& texture)
{
    sourceTexture = texture;
    textureEnabled = texture.isValid();
    rebuildPrebake();
}

void TrackRenderer::clearTexture()
{
    sourceTexture = {};
    textureEnabled = false;
    prebaked.valid = false;
}
