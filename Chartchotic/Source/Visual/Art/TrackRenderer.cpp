/*
    ==============================================================================

        TrackRenderer.cpp
        Author:  Noah Baxter

    ==============================================================================
*/

#include "TrackRenderer.h"
#include "../Geometry/RenderTypeConfig.h"
#include "../Utils/LaneColours.h"
#include "ProceduralTrackArt.h"

using namespace PositionConstants;

// Vertical arc shared by the strikeline pads/grooves and the lane-lines so both bow
// up at the board centre by the same amount (matching the bar/gem curve). Without
// this the flat lane-lines diverge from the arced grooves, worst toward the edges.
// arc = curv * fretboardWidth * (1 - dist^2); up at centre.
static int strikeGroupKey(Part part, int lane);   // defined below; used by lane-lines + pads

static float strikelineArcY(const LaneCorners& e, float x)
{
    const float arcFrac = -NOTE_CURVATURE;   // +0.02, magnitude of the bar curve
    float c  = (e.leftX + e.rightX) * 0.5f;
    float wd = (e.rightX - e.leftX) * FRETBOARD_SCALE;
    float d  = juce::jlimit(-1.0f, 1.0f, (x - c) / (wd * 0.5f));
    return e.centerY - arcFrac * wd * (1.0f - d * d);
}

TrackRenderer::TrackRenderer(juce::ValueTree& state)
    : state(state)
{
#ifndef CHARTCHOTIC_NO_BINARY_DATA
    // Load layer images from BinaryData
    strikelineConnectorsImage = juce::ImageCache::getFromMemory(BinaryData::strikeline_connectors_png, BinaryData::strikeline_connectors_pngSize);
    kickSmashersImage = juce::ImageCache::getFromMemory(BinaryData::kick_smashers_png, BinaryData::kick_smashers_pngSize);
#endif
}

void TrackRenderer::paint(juce::Graphics& g, int viewportWidth, int viewportHeight)
{
    if (PositionMath::bemaniMode)
    {
        bool showTrack = !state.hasProperty("showTrack") || (bool)state["showTrack"];
        if (showTrack)
        {
            auto edge = PositionMath::getFretboardEdge(getRenderType(activePart), 0.0f, viewportWidth, viewportHeight,
                            HIGHWAY_POS_START, cached.posEnd);
            g.setColour(TrackColours::highwayFill);
            g.fillRect(edge.leftX, 0.0f, edge.rightX - edge.leftX, (float)viewportHeight);
        }
        return;
    }

    if (!fadedTrackImage.isValid()) return;

    // Stretch if baked size doesn't match (during resize, before rebuild settles)
    if (fadedTrackImage.getWidth() == viewportWidth && fadedTrackImage.getHeight() == viewportHeight)
        g.drawImageAt(fadedTrackImage, 0, 0);
    else
        g.drawImage(fadedTrackImage, 0, 0, viewportWidth, viewportHeight,
                    0, 0, fadedTrackImage.getWidth(), fadedTrackImage.getHeight());
}

void TrackRenderer::paintBemaniOverlay(juce::Graphics& g, int viewportWidth, int viewportHeight)
{
    if (!PositionMath::bemaniMode) return;

    bool isDrums = isDrumLike(activePart);
    const auto* config = getRenderTypeConfig(getRenderType(activePart));
    auto edge = PositionMath::getFretboardEdge(getRenderType(activePart), 0.0f, viewportWidth, viewportHeight,
                    HIGHWAY_POS_START, cached.posEnd);
    float leftX = edge.leftX;
    float rightX = edge.rightX;
    float fbWidth = rightX - leftX;
    float h = (float)viewportHeight;

    // Lane dividers
    bool showLaneSeps = !state.hasProperty("showLaneSeparators") || (bool)state["showLaneSeparators"];
    bool showStrike = !state.hasProperty("showStrikeline") || (bool)state["showStrikeline"];

    if (showLaneSeps)
    {
        int numCols = (int)config->laneCount - 1;
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
    if (showStrike)
    {
        float strikeAlpha = bemaniConfig.strikelineOpacity;
        float strikeFrac = bemaniConfig.strikelinePos;
        int numCols = (int)config->laneCount - 1;
        float colW = fbWidth / (float)numCols;
        float strikeY = h * strikeFrac;
        float padH = colW * 0.55f;   // square-ish pads
        float padY = strikeY - padH * 0.5f;
        float inset = colW * 0.08f;
        float corner = colW * 0.15f;

        // Guitar: Green Red Yellow Blue Orange
        // Drums:  Red Yellow Blue Green
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

            // Outer rounded square — gem color, semi-transparent
            g.setColour(cols[i].withAlpha(strikeAlpha * 0.5f));
            g.fillRoundedRectangle(padRect, corner);

            // Darker inner rectangle — darker center chunk
            float innerInset = pw * 0.15f;
            auto innerRect = padRect.reduced(innerInset, padH * 0.2f);
            g.setColour(cols[i].darker(0.6f).withAlpha(strikeAlpha * 0.7f));
            g.fillRoundedRectangle(innerRect, corner * 0.5f);

            // Thin bright border
            g.setColour(cols[i].withAlpha(strikeAlpha * 0.8f));
            g.drawRoundedRectangle(padRect, corner, 1.0f);
        }

        // Thin white line through center of strikeline
        g.setColour(juce::Colours::white.withAlpha(strikeAlpha * 0.3f));
        g.drawHorizontalLine((int)strikeY, leftX, rightX);
    }

}

void TrackRenderer::paintBemaniSidebars(juce::Graphics& g, int viewportWidth, int viewportHeight)
{
    if (!PositionMath::bemaniMode) return;

    auto edge = PositionMath::getFretboardEdge(getRenderType(activePart), 0.0f, viewportWidth, viewportHeight,
                    HIGHWAY_POS_START, cached.posEnd);
    float leftX = edge.leftX;
    float rightX = edge.rightX;
    float h = (float)viewportHeight;
    float viewW = (float)viewportWidth;

    // Padding lets note glyphs bleed slightly past the fretboard edge
    float pad = (rightX - leftX) * 0.06f;
    float maskL = leftX - pad;
    float maskR = rightX + pad;

    // Opaque black masks outside the padded fretboard (extend far to cover scaled area)
    g.setColour(juce::Colours::black);
    g.fillRect(-viewW, -h, maskL + viewW, h * 3.0f);
    g.fillRect(maskR, -h, viewW * 2.0f, h * 3.0f);

}

void TrackRenderer::paintBemaniRails(juce::Graphics& g, int viewportWidth, int viewportHeight)
{
    if (!PositionMath::bemaniMode) return;

    auto edge = PositionMath::getFretboardEdge(getRenderType(activePart), 0.0f, viewportWidth, viewportHeight,
                    HIGHWAY_POS_START, cached.posEnd);
    float leftX = edge.leftX;
    float rightX = edge.rightX;
    float h = (float)viewportHeight;

    // Sidebar rails — outer grey (narrow), black (wide), inner grey (wider)
    float fbW = rightX - leftX;
    float railInset = bemaniConfig.railInset * fbW;
    float outerGreyW = std::max(1.0f, fbW * 0.004f);
    float blackW     = std::max(2.0f, fbW * 0.012f);
    float innerGreyW = std::max(1.5f, fbW * 0.006f);
    float totalRailW = outerGreyW + blackW + innerGreyW;

    auto innerGrey = TrackColours::padBevelInner;
    auto outerGrey = TrackColours::padBevelOuter;
    auto darkCol   = TrackColours::padBevelDark;

    // Left rail (outer→inner = left→right)
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

    // Right rail (inner→outer = left→right, mirrored)
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

void TrackRenderer::paintTexture(juce::Graphics& g, float scrollOffset, int targetW, int targetH)
{
    if (PositionMath::bemaniMode)
    {
        if (!textureEnabled || !sourceTexture.isValid()) return;

        bool isDrums = isDrumLike(activePart);
        auto edge = PositionMath::getFretboardEdge(getRenderType(activePart), 0.0f, targetW, targetH,
                        HIGHWAY_POS_START, cached.posEnd);
        float leftX = edge.leftX;
        float hwyW = edge.rightX - edge.leftX;
        if (hwyW <= 0) return;

        // Tile the texture vertically within the highway bounds, scrolling
        float texAspect = (float)sourceTexture.getWidth() / (float)sourceTexture.getHeight();
        float tileW = hwyW;
        float tileH = tileW / texAspect * textureScale;
        if (tileH < 1.0f) return;

        // scrollOffset = how many viewport-heights of notes have scrolled by.
        // Match PositionMath Bemani Y: pixelsPerUnit = REFERENCE_HEIGHT * strikelinePos / bemaniHwyScale.
        // scrollOffset is in position units, so totalPx = scrollOffset * pixelsPerUnit * texSpeed.
        float strikeFrac = bemaniConfig.strikelinePos;
        float texSpeed = bemaniConfig.texSpeed;
        float hwyScale = std::max(0.1f, PositionMath::bemaniHwyScale);
        // Total pixels scrolled (continuous, never wraps)
        float totalPx = scrollOffset * strikeFrac * PositionConstants::REFERENCE_HEIGHT * texSpeed / hwyScale;
        // Modulo against tile height for seamless repeat
        float offset = std::fmod(totalPx, tileH);
        if (offset < 0.0f) offset += tileH;

        // Tile from bottom to top, covering entire viewport
        g.saveState();
        g.reduceClipRegion((int)leftX, 0, (int)std::ceil(hwyW), targetH);
        g.setOpacity(textureOpacity);
        float startY = (float)targetH - offset;
        for (float y = startY; y > -tileH; y -= tileH)
            g.drawImage(sourceTexture, leftX, y, tileW, tileH,
                        0, 0, sourceTexture.getWidth(), sourceTexture.getHeight());
        g.restoreState();
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
    g.setColour(PositionMath::debugPolyShade ? TrackColours::debugPoly : TrackColours::highwayFill);
#else
    g.setColour(TrackColours::highwayFill);
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
        getRenderType(activePart) == cached.renderType)
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
        cached.renderType = getRenderType(activePart);
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

    auto edgeNear = PositionMath::getFretboardEdge(getRenderType(activePart), HIGHWAY_POS_START, width, height,
                                                    HIGHWAY_POS_START, posEnd);
    auto edgeFar = PositionMath::getFretboardEdge(getRenderType(activePart), effectiveEnd, width, height,
                                                   HIGHWAY_POS_START, posEnd);
    int pixelHeight = std::max(1, (int)(edgeNear.centerY - edgeFar.centerY));
    cached.stripCount = std::clamp(pixelHeight / PIXELS_PER_STRIP, MIN_STRIPS, MAX_STRIPS);

    cached.edges.resize(cached.stripCount + 1);
    for (int i = 0; i <= cached.stripCount; i++)
    {
        float pos = HIGHWAY_POS_START + posRange * (float)i / (float)cached.stripCount;
        auto edge = PositionMath::getFretboardEdge(getRenderType(activePart), pos, width, height,
                                                    HIGHWAY_POS_START, posEnd);
        edge.centerY += (float)overflow;  // offset into taller bitmap
        cached.edges[i] = { edge, pos };
    }

    cached.width = width;
    cached.height = height;
    cached.overflow = overflow;
    cached.isDrums = isDrums;
    cached.renderType = getRenderType(activePart);
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
        // Side rails are stroked procedurally along the actual board edges for every
        // part (they replaced the fixed sidebars PNG, which could only match one board
        // width).
        bakeSidebarRailsPerspective(width, totalH, overflow, isDrums, farFadeEnd, farFadeLen, farFadeCurve, posEnd);
        bakeLaneLinesPerspective(width, totalH, overflow, isDrums,
                                  farFadeEnd, farFadeLen, farFadeCurve, posEnd);
        // Strikeline pads are drawn procedurally for every part (they replaced the
        // fixed strikeline PNGs).
        bakeStrikelinePadsPerspective(width, totalH, overflow, isDrums, farFadeEnd, farFadeLen, farFadeCurve, posEnd);
        // Kick smashers (CONNECTORS) are a fixed-width PNG scaled to the RENDER width, so unlike
        // the procedural rails/lanes/strikeline they don't follow the wider elite board coords.
        // Widen the transform by the same board factor so the end-caps sit on the elite board edge
        // instead of floating inside it.
        LayerTransform connectors = layers[CONNECTORS];
        if (activePart == Part::ELITE_DRUMS)
            connectors.scale *= PositionConstants::ELITE_BOARD_WIDTH_SCALE;
        bakeLayerImage(layerImages[CONNECTORS], isDrums ? kickSmashersImage : strikelineConnectorsImage, connectors,
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

    const auto* config = getRenderTypeConfig(getRenderType(activePart));
    const auto& fbCoords = *config->fretboardCoords;

    // Compute boundary fractions between adjacent inner lanes (skip bar lane 0). Skip the
    // boundary between two lanes in the same strike group (e.g. within the tom group) so
    // the merged pad reads as one lane with no divider inside it.
    std::vector<float> boundaryFracs;
    for (int i = 1; i < laneCount_ - 1; i++)
    {
        if (strikeGroupKey(activePart, i) == strikeGroupKey(activePart, i + 1))
            continue;
        float rightNorm = laneCoords_[i].normX1 + laneCoords_[i].normWidth1;
        float leftNorm = laneCoords_[i + 1].normX1;
        float midNorm = (rightNorm + leftNorm) * 0.5f;
        boundaryFracs.push_back((midNorm - fbCoords.normX1) / fbCoords.normWidth1);
    }

    if (boundaryFracs.empty()) { out = {}; return; }

    {
        juce::Graphics g(out);
        g.setColour(TrackColours::sheenWhite);

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
                float y = strikelineArcY(edge, x);

                if (!started) {
                    path.startNewSubPath(x, y);
                    started = true;
                } else {
                    path.lineTo(x, y);
                }
            }

            g.strokePath(path, juce::PathStrokeType(1.5f));
        }
    }

    applyFarFade(out, w, h, overflow, isDrums, farFadeEnd, farFadeLen, farFadeCurve,
                 posEnd);
}

void TrackRenderer::bakeSidebarRailsPerspective(int w, int h, int overflow, bool isDrums,
                                                 float farFadeEnd, float farFadeLen, float farFadeCurve,
                                                 float posEnd)
{
    auto& out = layerImages[SIDEBARS];
    out = juce::Image(juce::Image::ARGB, w, h, true);

    if (cached.stripCount < 1) { out = {}; return; }

    {
        juce::Graphics g(out);
        ProceduralTrackArt::drawSidebarRails(g, cached.edges, cached.stripCount);
    }

    applyFarFade(out, w, h, overflow, isDrums, farFadeEnd, farFadeLen, farFadeCurve,
                 posEnd);
}

// Per-lane strikeline pad colours: {top (duller), bottom (brighter)}, sampled from
// the reference PNG, which draws each pad as a vertical gradient.
struct PadColours { juce::Colour top, bottom; };

// Strikeline pad colours. The lane hues come from the shared LaneColours table (single
// source of truth, also drives gem tints); pad top = lane dark, bottom = lane bright.
namespace FretColour
{
    static PadColours pad(const LaneColours::Lane& l) { return { LaneColours::dark(l), LaneColours::bright(l) }; }
    const PadColours none     { juce::Colours::transparentBlack,          juce::Colours::transparentBlack };          // kick / open lane (undrawn)
    const PadColours red      = pad(LaneColours::red);
    const PadColours yellow   = pad(LaneColours::yellow);
    const PadColours blue     = pad(LaneColours::blue);
    const PadColours green    = pad(LaneColours::green);
    const PadColours orange   = pad(LaneColours::orange);
    const PadColours purple   = pad(LaneColours::purple);
    const PadColours white    = pad(LaneColours::white);
    const PadColours fallback { TrackColours::padFallbackDark, TrackColours::padFallbackLite };  // unmapped lane
}

// Map the shared lane tint (PositionConstants::ELITE_LANE_STYLES) to pad colours, so the
// strikeline and the gems read from the same colour scheme.
static PadColours tintToPad(PositionConstants::DrumLaneTint t)
{
    using namespace FretColour;
    using T = PositionConstants::DrumLaneTint;
    switch (t)
    {
        case T::Red:    return red;
        case T::Yellow: return yellow;
        case T::Purple: return purple;
        case T::Orange: return orange;
        case T::Blue:   return blue;
        case T::Green:  return green;
        case T::White:  return white;
        case T::None:   return none;
    }
    return fallback;
}

// Strikeline grouping key: consecutive hand lanes with the same key merge into one wide
// pad (no groove/divider between them) while their gems still render individually. Elite's
// toms all share the Orange tint, so an arbitrary run of toms reads as one tom group; every
// other lane gets a unique key and never merges.
static int strikeGroupKey(Part part, int lane)
{
    if (part == Part::ELITE_DRUMS && lane >= 0 && lane < 9)
        return (int) PositionConstants::ELITE_LANE_STYLES[lane].tint;
    return 1000 + lane;
}

static PadColours strikePadColours(Part part, int lane)
{
    using namespace FretColour;
    if (part == Part::ELITE_DRUMS)
        return (lane >= 0 && lane < 9) ? tintToPad(PositionConstants::ELITE_LANE_STYLES[lane].tint)
                                       : fallback;
    if (isGuitarLike(part))
    {
        // 5-fret GRYBO (lane 0 = open).
        static const PadColours g[6] = { none, green, red, yellow, blue, orange };
        return (lane >= 0 && lane < 6) ? g[lane] : fallback;
    }
    // 4-lane drums: kick, red, yellow, blue, green.
    static const PadColours d[5] = { none, red, yellow, blue, green };
    return (lane >= 0 && lane < 5) ? d[lane] : fallback;
}

void TrackRenderer::bakeStrikelinePadsPerspective(int w, int h, int overflow, bool isDrums,
                                                   float farFadeEnd, float farFadeLen, float farFadeCurve,
                                                   float posEnd)
{
    auto& out = layerImages[STRIKELINE];
    out = juce::Image(juce::Image::ARGB, w, h, true);

    if (!laneCoords_ || laneCount_ < 3 || cached.stripCount < 1) { out = {}; return; }

    const auto* config = getRenderTypeConfig(getRenderType(activePart));
    const auto& fb = *config->fretboardCoords;

    // Interpolate the cached board edge at an arbitrary highway position. The strips
    // are evenly spaced over [edges.front().pos, edges.back().pos] (which runs to
    // max(posEnd, farFadeEnd), NOT posEnd), so map pos->index against that actual span
    // -- using posEnd here would sample the wrong strip whenever farFadeEnd > posEnd,
    // scaling the strikeline slightly wider than the lane-lines (which read the strips
    // directly).
    auto edgeAt = [&](float pos)
    {
        float posStart = cached.edges.front().second;
        float posSpan  = cached.edges.back().second - posStart;
        float f = posSpan > 1e-6f ? (pos - posStart) / posSpan * (float)cached.stripCount : 0.0f;
        f = juce::jlimit(0.0f, (float)cached.stripCount, f);
        int i0 = (int)f, i1 = std::min(i0 + 1, cached.stripCount);
        float t = f - (float)i0;
        const auto& a = cached.edges[i0].first;
        const auto& b = cached.edges[i1].first;
        return PositionConstants::LaneCorners{
            a.leftX  + (b.leftX  - a.leftX)  * t,
            a.rightX + (b.rightX - a.rightX) * t,
            a.centerY + (b.centerY - a.centerY) * t };
    };
    auto xAtFrac = [](const PositionConstants::LaneCorners& e, float frac)
    {
        float c  = (e.leftX + e.rightX) * 0.5f;
        float wd = (e.rightX - e.leftX) * FRETBOARD_SCALE;
        return (c - wd * 0.5f) + frac * wd;
    };
    auto normToFrac = [&](float norm) { return (norm - fb.normX1) / fb.normWidth1; };

    // Arc the pad top/bottom edges to match the bar/gem curve (bows up at center).
    // Shared with the lane-lines (strikelineArcY) so grooves and lanes stay coincident.
    auto arcY = [](const PositionConstants::LaneCorners& e, float x) { return strikelineArcY(e, x); };

    // Pad band straddling the strike (pos 0). Tunable against the PNG.
    const float pNear = -0.034f, pFar = 0.012f;
    // Outer-end treatment: with caps, a silver end bar sits just outside the first/last
    // pad (drum/elite reference); without, the outer pads run out to the rail themselves
    // with only a thin margin (guitar reference). Single toggle -- flip per part or
    // hard-set either way.
    const bool drawEndCaps = false;      // was isDrumLike(activePart); but didnt like it as much
    const float gapInsetFrac = 0.105f;   // pad inset as a fraction of the average pad width (leaves the separator gap)
    auto eN = edgeAt(pNear);
    auto eF = edgeAt(pFar);

    const int firstHand = 1;
    const int lastHand  = laneCount_ - 1;

    // One inset shared by every pad (based on the average pad width), so the separator
    // gaps stay symmetric even when adjacent lanes differ in width (elite's wide cymbal
    // lanes next to narrow drum lanes). A per-pad-proportional inset would pull a wide
    // pad's colour block off-centre within its slot, leaving uneven space on each side.
    const float avgPadWidth = (laneCoords_[lastHand].normX1 + laneCoords_[lastHand].normWidth1
                               - laneCoords_[firstHand].normX1) / (float)(lastHand - firstHand + 1);
    const float padInset = avgPadWidth * gapInsetFrac;

    // Snap the outer pads to the side rails: their outermost mark (the end cap when
    // present, else the coloured pad edge) lands just inside the rail's inner edge,
    // which sits at a fixed board fraction for every part (see RailGeom). capExt is how
    // far the cap sticks out past the pad edge as a fraction of the pad's own width,
    // matching chromeBar's end-cap width. Solving in colour-edge fraction space keeps
    // both sides symmetric by construction (no per-part recentre/pull needed).
    const float railInnerFrac = ProceduralTrackArt::RailGeom::innerEdgeFraction();
    const float outerGapFrac  = 0.010f;   // dark gap between rail inner edge and cap/pad
    const float capExt        = drawEndCaps ? 0.14f : 0.0f;
    const float insetFrac     = padInset / fb.normWidth1;
    // Given the pad's inner coloured-edge fraction, return the outer boundary (pre-inset)
    // norm so the outermost mark lands at the rail. leftEdge picks which rail.
    auto outerBoundaryNorm = [&](bool leftEdge, float innerFrac)
    {
        float target = leftEdge ? railInnerFrac + outerGapFrac
                                : (1.0f - railInnerFrac) - outerGapFrac;
        // cap outer = edge -/+ capExt*(padWidth); solve edge frac, then undo the inset.
        float edge = (target + capExt * innerFrac) / (1.0f + capExt);
        return fb.normX1 + (leftEdge ? edge - insetFrac : edge + insetFrac) * fb.normWidth1;
    };

    std::vector<ProceduralTrackArt::StrikePad> pads;
    // Walk the hand lanes in groups: a run of consecutive lanes sharing a strike-group
    // key (e.g. the orange toms) becomes ONE wide pad with no internal groove, but the
    // lanes themselves stay separate so their gems still render individually.
    for (int gi = firstHand; gi <= lastHand; )
    {
        int gj = gi;
        while (gj < lastHand && strikeGroupKey(activePart, gj + 1) == strikeGroupKey(activePart, gi))
            gj++;

        float leftNorm  = laneCoords_[gi].normX1;                                   // group's left lane
        float rightNorm = laneCoords_[gj].normX1 + laneCoords_[gj].normWidth1;      // group's right lane

        // Group boundaries: midpoint to the neighbouring lane outside the group. The
        // exterior boundary (first/last group) is snapped to the side rail so the row
        // fills the board.
        float padLNorm, padRNorm;
        if (gi > firstHand)  padLNorm = (laneCoords_[gi - 1].normX1 + laneCoords_[gi - 1].normWidth1 + leftNorm) * 0.5f;
        if (gj < lastHand)   padRNorm = (rightNorm + laneCoords_[gj + 1].normX1) * 0.5f;
        if (gi == firstHand) padLNorm = outerBoundaryNorm(true,  normToFrac(padRNorm - padInset));
        if (gj == lastHand)  padRNorm = outerBoundaryNorm(false, normToFrac(padLNorm + padInset));

        float lf = normToFrac(padLNorm + padInset);
        float rf = normToFrac(padRNorm - padInset);
        // Perspective-projected edges (near wider than far) so the strip tapers to
        // match the reference; Y also carries the arc bow.
        float xnl = xAtFrac(eN, lf), xnr = xAtFrac(eN, rf);
        float xfl = xAtFrac(eF, lf), xfr = xAtFrac(eF, rf);

        // Right lane boundary (pre-inset) = the lane-line position; the connector
        // groove is centred here so it lines up with the gridline.
        float bf  = normToFrac(padRNorm);
        float xnb = xAtFrac(eN, bf), xfb = xAtFrac(eF, bf);

        ProceduralTrackArt::StrikePad pad;
        pad.nearL = { xnl, arcY(eN, xnl) };
        pad.nearR = { xnr, arcY(eN, xnr) };
        pad.farL  = { xfl, arcY(eF, xfl) };
        pad.farR  = { xfr, arcY(eF, xfr) };
        pad.sepNear = { xnb, arcY(eN, xnb) };
        pad.sepFar  = { xfb, arcY(eF, xfb) };

        // Sample the top/bottom edges along the arc so wide (grouped) pads bow with the
        // board instead of drawing a straight chord between the corners.
        const int kEdge = 14;
        for (int k = 0; k < kEdge; k++)
        {
            float f  = lf + (rf - lf) * (float)k / (float)(kEdge - 1);
            float xf = xAtFrac(eF, f), xn = xAtFrac(eN, f);
            pad.farEdge.push_back ({ xf, arcY(eF, xf) });
            pad.nearEdge.push_back({ xn, arcY(eN, xn) });
        }

        auto pc = strikePadColours(activePart, gi);
        pad.baseColour = pc.top;
        pad.bottomColour = pc.bottom;
        pads.push_back(pad);

        gi = gj + 1;
    }

    {
        juce::Graphics g(out);
        // Guitar's narrower lanes need wider chrome bars to read the same as the
        // wider-lane references.
        const float barHalfFrac = isGuitarLike(activePart) ? 0.35f : 0.28f;
        ProceduralTrackArt::drawStrikelinePads(g, pads, drawEndCaps, barHalfFrac);   // builds its own arc-hugging dark frame
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
