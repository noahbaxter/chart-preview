/*
    ==============================================================================

        NotePainter.cpp
        Author:  Noah Baxter

        Note overlay geometry and curved image cache. Extracted from
        HighwayComponent + NoteRenderer.

    ==============================================================================
*/

#include "NotePainter.h"

#include <map>
#include <tuple>
#include <vector>
#include <cmath>

using namespace PositionConstants;

namespace
{
    // Process-wide curved image cache. All plugin instances in the same host process
    // share this cache. Safe because curvature is a global tuning constant, not
    // per-instance state. If curvature ever becomes per-instance, key by curvature
    // value or make the cache instance-scoped.
    using CurveKey = std::tuple<juce::Image*, int, bool>;
    std::map<CurveKey, NotePainter::CurvedImageEntry> s_curvedCache;
    float s_lastCachedCurvatureGuitar = PositionConstants::NOTE_CURVATURE;
    float s_lastCachedCurvatureDrums  = PositionConstants::NOTE_CURVATURE;
}

namespace NotePainter
{

// =============================================================================
// NoteRenderContext::buildGemParams
// =============================================================================

GemParams NoteRenderContext::buildGemParams(float position, uint gemColumn,
                                            float imageAspect,
                                            Gem gemType, bool starPower,
                                            float userScale) const
{
    bool isBar = isBarNote(gemColumn, activePart);
    float sizeScale = isBar ? BAR_SIZE : GEM_SIZE;

    // Foreshorten
    float foreshorten = 1.0f;
    if (depthForeshorten > 0.0f && !PositionMath::bemaniMode)
    {
        float adjustedPos = isBar ? position + BAR_NOTE_POS_OFFSET : position;
#ifdef DEBUG
        const auto& pp = PositionMath::perspParams(isDrums);
#else
        auto pp = getPerspectiveParams(isDrums);
#endif
        float depth = std::max(0.0f, adjustedPos) / pp.vanishingPointDepth;
        float scaleNear = 1.0f + (pp.highwayDepth / pp.playerDistance) * pp.perspectiveStrength;
        float psCur = scaleNear / (1.0f + depth * (scaleNear - 1.0f));
        float rawRatio = psCur / scaleNear;
        foreshorten = 1.0f - (1.0f - rawRatio) * depthForeshorten;
    }

    // Curvature
    float noteCurv = isDrums ? noteCurvatureDrums : noteCurvatureGuitar;
    float baseCurv = isBar ? BAR_CURVATURE : noteCurv;
    float curvature = PositionMath::bemaniMode ? baseCurv * bemaniConfig.curvature : baseCurv;

    // Base scale
    float baseW, baseH;
    if (PositionMath::bemaniMode)
    {
        baseW = isBar ? bemaniConfig.barW : bemaniConfig.gemW;
        baseH = isBar ? bemaniConfig.barH : bemaniConfig.gemH;
    }
    else
    {
        const auto& bs = isBar ? barScale : gemScale;
        baseW = bs.width;
        baseH = bs.height;
    }

    // Per-note-type scale
    float typeScale = 1.0f;
    if (!isBar)
    {
        if (!isDrums)
        {
            switch (gemType) {
            case Gem::NOTE:        typeScale = gemTypeScales.normal; break;
            case Gem::HOPO_GHOST:  typeScale = gemTypeScales.hopo; break;
            case Gem::TAP_ACCENT:  typeScale = gemTypeScales.gTap; break;
            default: break;
            }
        }
        else
        {
            switch (gemType) {
            case Gem::NOTE:        typeScale = gemTypeScales.normal; break;
            case Gem::HOPO_GHOST:  typeScale = gemTypeScales.dGhost; break;
            case Gem::TAP_ACCENT:  typeScale = gemTypeScales.dAccent; break;
            case Gem::CYM:         typeScale = gemTypeScales.cymbal; break;
            case Gem::CYM_GHOST:   typeScale = gemTypeScales.cGhost; break;
            case Gem::CYM_ACCENT:  typeScale = gemTypeScales.cAccent; break;
            default: break;
            }
        }

        // Star power multiplier
        if (starPower && std::abs(gemTypeScales.spGem - 1.0f) > 0.001f)
            typeScale *= gemTypeScales.spGem;
    }

    float wScale = baseW * typeScale;
    float hScale = baseH * typeScale;

    // Per-column adjustments + Z offset + strikeline width (perspective only)
    float rawZOff = 0.0f;
    float strikeWidth = 1.0f;
    NormalizedCoordinates laneCoords{};
    int bemaniLaneIdx = -1;

    if (!PositionMath::bemaniMode)
    {
        rawZOff = isBar ? barZOffset : gemZOffset;

        if (!isBar)
        {
            float colSNear = 1.0f, colSFar = 1.0f, colW = 1.0f, colH = 1.0f;
            if (!isDrums && gemColumn < GUITAR_LANE_COUNT && guitarColAdjust) {
                const auto& ca = guitarColAdjust[gemColumn];
                colSNear = ca.sNear; colSFar = ca.sFar; colW = ca.w; colH = ca.h;
            } else if (isDrums && drumColAdjust) {
                uint drumIdx = drumColumnIndex(gemColumn);
                const auto& ca = drumColAdjust[drumIdx];
                colSNear = ca.sNear; colSFar = ca.sFar; colW = ca.w; colH = ca.h;
                rawZOff += ca.z;
            }

#ifdef DEBUG
            float vpDepth = PositionMath::perspParams(isDrums).vanishingPointDepth;
#else
            float vpDepth = getPerspectiveParams(isDrums).vanishingPointDepth;
#endif
            float t = juce::jlimit(0.0f, 1.0f, position / vpDepth);
            float colScale = colSNear + (colSFar - colSNear) * t;
            wScale *= colScale * colW;
            hScale *= colScale * colH;
        }

        // Strike width for perspective Z scaling
        if (isBar)
        {
            auto fbStrike = PositionMath::getFretboardEdge(isDrums, 0.0f,
                viewportW, viewportH, HIGHWAY_POS_START, posEnd);
            strikeWidth = (fbStrike.rightX - fbStrike.leftX) * BAR_FRETBOARD_FIT * BAR_SIZE;
        }
        else if (laneCoordsGuitar && laneCoordsDrums)
        {
            const auto& colCoordsRef = isGuitarLike(activePart)
                ? laneCoordsGuitar[(gemColumn < GUITAR_LANE_COUNT) ? gemColumn : 1]
                : laneCoordsDrums[drumColumnIndex(gemColumn)];
            auto strikeEdge = PositionMath::getColumnPosition(isDrums, 0.0f,
                viewportW, viewportH, HIGHWAY_POS_START, posEnd,
                colCoordsRef, GEM_SIZE, FRETBOARD_SCALE);
            strikeWidth = strikeEdge.rightX - strikeEdge.leftX;
        }
    }

    // Lane coords
    if (!isBar)
    {
        if (!isDrums && laneCoordsGuitar) {
            int idx = (gemColumn < GUITAR_LANE_COUNT) ? (int)gemColumn : 1;
            laneCoords = laneCoordsGuitar[idx];
            bemaniLaneIdx = idx - 1;
        } else if (isDrums && laneCoordsDrums) {
            uint drumIdx = drumColumnIndex(gemColumn);
            laneCoords = laneCoordsDrums[drumIdx];
            bemaniLaneIdx = (int)drumIdx - 1;
        }
    }

    // Bemani nudge
    float bemaniNudgeY = 0.0f;
    if (PositionMath::bemaniMode)
    {
        float nudge = isBar ? bemaniConfig.barNudge : bemaniConfig.gemNudge(isDrums);
        float pixelsPerUnit = REFERENCE_HEIGHT * bemaniConfig.strikelinePos
                            / std::max(0.1f, PositionMath::bemaniHwyScale);
        bemaniNudgeY = pixelsPerUnit * nudge;
    }

    GemParams gp;
    gp.position = position;
    gp.gemColumn = (int)gemColumn;
    gp.isBar = isBar;
    gp.isDrums = isDrums;
    gp.viewportW = viewportW;
    gp.viewportH = viewportH;
    gp.posEnd = posEnd;
    gp.imageAspect = imageAspect;
    gp.sizeScale = sizeScale;
    gp.userScale = userScale;
    gp.wScale = wScale;
    gp.hScale = hScale;
    gp.foreshorten = foreshorten;
    gp.rawZOffset = rawZOff;
    gp.strikeWidth = strikeWidth;
    gp.curvature = curvature;
    gp.laneCoords = laneCoords;
    gp.bemaniLaneIdx = bemaniLaneIdx;
    gp.bemaniNudgeY = bemaniNudgeY;
    gp.hasOverlay = false;
    gp.overlayAdj = {};
    return gp;
}

// =============================================================================
// Ghost cursor / selection overlay
// =============================================================================

NoteRect computeRect(float position, int lane, Part activePart,
                     int renderW, int renderH, float posEnd, int topOverflow,
                     bool stretchToFill, int componentW, int componentH,
                     float foreshorten, float gemZOffset, float barZOffset,
                     float noteCurvature)
{
    NoteRect nr{};
    bool isDrums = isDrumLike(activePart);
    nr.isBar = (lane == 0);

    int totalH = renderH + topOverflow;

    // Screen transform (mirrors HighwayComponent::paint())
    float sx, sy, ox, oy;
    if (stretchToFill && !PositionMath::bemaniMode)
    {
        sx = (float)componentW / (float)renderW;
        sy = (float)componentH / (float)totalH;
        ox = 0.0f; oy = 0.0f;
    }
    else
    {
        float scale = std::min((float)componentW / (float)renderW,
                               (float)componentH / (float)totalH);
        sx = scale; sy = scale;
        ox = ((float)componentW - (float)renderW * scale) / 2.0f;
        oy = (float)componentH - (float)totalH * scale;
    }
    nr.sy = sy;

    // Note rect in render space
    float rLeftX, rRightX, rCenterY, noteW, noteH;
    float renderPosition = nr.isBar ? position + BAR_NOTE_POS_OFFSET : position;
    if (nr.isBar)
    {
        auto fbEdge = PositionMath::getFretboardEdge(
            isDrums, renderPosition, (uint)renderW, (uint)renderH,
            HIGHWAY_POS_START, HIGHWAY_POS_END);
        float fbWidth = fbEdge.rightX - fbEdge.leftX;
        noteW = fbWidth * BAR_FRETBOARD_FIT * BAR_SIZE;
        float cx = (fbEdge.leftX + fbEdge.rightX) * 0.5f;
        rLeftX = cx - noteW * 0.5f;
        rRightX = cx + noteW * 0.5f;
        rCenterY = fbEdge.centerY;
        noteH = (noteW / 16.0f) * foreshorten;
    }
    else
    {
        const auto* laneCoords = isDrums ? drumBezierLaneCoords : guitarBezierLaneCoords;
        auto corners = PositionMath::getColumnPosition(
            isDrums, position, (uint)renderW, (uint)renderH,
            HIGHWAY_POS_START, HIGHWAY_POS_END,
            laneCoords[lane], GEM_SIZE, FRETBOARD_SCALE,
            PositionMath::bemaniMode ? lane : -1);
        rLeftX = corners.leftX;
        rRightX = corners.rightX;
        rCenterY = corners.centerY;
        noteW = rRightX - rLeftX;
        noteH = (noteW / 2.0f) * GEM_SCALE.height * foreshorten;
    }

    // Neck curvature
    nr.curvature = noteCurvature;
    nr.arcOffset = 0.0f;
    if (nr.curvature != 0.0f)
    {
        const auto& fbCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;
        float fbCenter = fbCoords.normX1 + fbCoords.normWidth1 * 0.5f;
        float fbHalfW = fbCoords.normWidth1 * 0.5f;
        float dist = 0.0f;
        if (!nr.isBar)
        {
            const auto* lc = isDrums ? drumBezierLaneCoords : guitarBezierLaneCoords;
            float colCenter = lc[lane].normX1 + lc[lane].normWidth1 * 0.5f;
            dist = (colCenter - fbCenter) / fbHalfW;
        }
        auto fbEdge = PositionMath::getFretboardEdge(
            isDrums, renderPosition, (uint)renderW, (uint)renderH,
            HIGHWAY_POS_START, HIGHWAY_POS_END);
        float fbWidthPx = (fbEdge.rightX - fbEdge.leftX) * FRETBOARD_SCALE;
        nr.arcOffset = fbWidthPx * nr.curvature * (1.0f - dist * dist);
    }

    nr.renderLeftX = rLeftX;
    nr.renderRightX = rRightX;
    nr.position = renderPosition;

    // Z offset
    float zOff = nr.isBar ? barZOffset : gemZOffset;

    // Transform to screen
    nr.screenLeftX  = rLeftX * sx + ox;
    nr.screenRightX = rRightX * sx + ox;
    nr.screenCenterY = (rCenterY + zOff + nr.arcOffset + (float)topOverflow) * sy + oy;
    nr.screenH = noteH * sy;

    return nr;
}

juce::Path buildCurvedPath(const NoteRect& nr, Part activePart,
                           int renderW, int renderH,
                           float expand)
{
    juce::Path p;
    float left  = nr.screenLeftX - expand;
    float right = nr.screenRightX + expand;
    float top   = nr.screenCenterY - nr.screenH * 0.5f - expand;
    float bot   = nr.screenCenterY + nr.screenH * 0.5f + expand;

    if (std::abs(nr.curvature) < 0.001f)
    {
        p.addRoundedRectangle(left, top, right - left, bot - top,
                              nr.isBar ? 2.0f : 3.0f);
    }
    else
    {
        bool isDrums = isDrumLike(activePart);
        auto fbEdge = PositionMath::getFretboardEdge(
            isDrums, nr.position, (uint)renderW, (uint)renderH,
            HIGHWAY_POS_START, HIGHWAY_POS_END);
        float fbWidthPx = (fbEdge.rightX - fbEdge.leftX) * FRETBOARD_SCALE;
        const auto& fbCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;
        float fbCenterNorm = fbCoords.normX1 + fbCoords.normWidth1 * 0.5f;
        float fbHalfWNorm = fbCoords.normWidth1 * 0.5f;

        float pts[3] = { nr.renderLeftX, (nr.renderLeftX + nr.renderRightX) * 0.5f, nr.renderRightX };
        float yOff[3];
        for (int i = 0; i < 3; i++)
        {
            float normX = pts[i] / (float)renderW;
            float d = (normX - fbCenterNorm) / fbHalfWNorm;
            float fullArc = fbWidthPx * nr.curvature * (1.0f - d * d);
            yOff[i] = (fullArc - nr.arcOffset) * nr.sy;
        }

        float midX = (left + right) * 0.5f;
        p.startNewSubPath(left, top + yOff[0]);
        p.quadraticTo(midX, top + yOff[1], right, top + yOff[2]);
        p.lineTo(right, bot + yOff[2]);
        p.quadraticTo(midX, bot + yOff[1], left, bot + yOff[0]);
        p.closeSubPath();
    }
    return p;
}

// =========================================================================
// Gem rect computation
// =========================================================================

juce::Rectangle<float> scaleRect(juce::Rectangle<float> r,
                                 float wScale, float hScale, float yOff)
{
    float cx = r.getCentreX();
    float cy = r.getCentreY() + yOff;
    float fw = r.getWidth() * wScale;
    float fh = r.getHeight() * hScale;
    return juce::Rectangle<float>(cx - fw * 0.5f, cy - fh * 0.5f, fw, fh);
}

float getColumnDistFromCenter(const NormalizedCoordinates& colCoords, bool isDrums)
{
    const auto& fbCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;
    float fbCenter = fbCoords.normX1 + fbCoords.normWidth1 * 0.5f;
    float fbHalfW = fbCoords.normWidth1 * 0.5f;
    float colCenter = colCoords.normX1 + colCoords.normWidth1 * 0.5f;
    return (colCenter - fbCenter) / fbHalfW;
}

juce::Rectangle<float> getOverlayGlyphRect(juce::Rectangle<float> glyphRect,
                                            const OverlayAdjust& adj)
{
    float sx = adj.scaleX * adj.scale;
    float sy = adj.scaleY * adj.scale;

    if (std::abs(sx - 1.0f) < 0.0001f && std::abs(sy - 1.0f) < 0.0001f)
        return glyphRect;

    float cx = glyphRect.getCentreX();
    float cy = glyphRect.getCentreY();
    float newW = glyphRect.getWidth() * sx;
    float newH = glyphRect.getHeight() * sy;
    return juce::Rectangle<float>(cx - newW / 2.0f, cy - newH / 2.0f, newW, newH);
}

GemRects computeGemRects(const GemParams& p)
{
    GemRects result{};

    float adjustedPosition = p.isBar ? p.position + BAR_NOTE_POS_OFFSET : p.position;

    // Base glyph rect
    juce::Rectangle<float> glyphRect;
    if (p.isBar)
    {
        if (PositionMath::bemaniMode)
        {
            glyphRect = PositionMath::computeBemaniBarRect(
                p.isDrums, adjustedPosition, p.viewportW, p.viewportH, p.posEnd,
                p.sizeScale, p.imageAspect, p.foreshorten);
        }
        else
        {
            auto fbEdge = PositionMath::getFretboardEdge(p.isDrums, adjustedPosition, p.viewportW, p.viewportH,
                                                          HIGHWAY_POS_START, p.posEnd);
            float fbWidth = fbEdge.rightX - fbEdge.leftX;
            float colWidth = fbWidth * BAR_FRETBOARD_FIT * p.sizeScale;
            float colHeight = (colWidth / p.imageAspect) * p.foreshorten;
            float cx = (fbEdge.leftX + fbEdge.rightX) * 0.5f;
            glyphRect = juce::Rectangle<float>(cx - colWidth * 0.5f, fbEdge.centerY - colHeight * 0.5f, colWidth, colHeight);
        }
    }
    else
    {
        auto edge = PositionMath::getColumnPosition(
            p.isDrums, adjustedPosition, p.viewportW, p.viewportH,
            HIGHWAY_POS_START, p.posEnd,
            p.laneCoords, 1.0f, FRETBOARD_SCALE, p.bemaniLaneIdx);
        float laneWidth = edge.rightX - edge.leftX;
        float colWidth = laneWidth * p.sizeScale;
        float colHeight = (colWidth / p.imageAspect) * p.foreshorten;
        float cx = (edge.leftX + edge.rightX) * 0.5f;
        glyphRect = juce::Rectangle<float>(cx - colWidth * 0.5f, edge.centerY - colHeight * 0.5f, colWidth, colHeight);
    }

    // User scale
    if (std::abs(p.userScale - 1.0f) > 0.001f)
        glyphRect = scaleRect(glyphRect, p.userScale, p.userScale, 0.0f);

    // Bemani nudge
    if (std::abs(p.bemaniNudgeY) > 0.001f)
        glyphRect.translate(0.0f, p.bemaniNudgeY);

    result.glyphRect = glyphRect;

    // Perspective-scale Z offset
    float zOff = p.rawZOffset;
    if (p.strikeWidth > 0.0f && std::abs(zOff) > 0.001f)
        zOff *= glyphRect.getWidth() / p.strikeWidth;
    result.perspZOffset = zOff;

    // Arc offset
    result.arcOffset = 0.0f;
    if (p.curvature != 0.0f && !p.isBar)
    {
        float dist = getColumnDistFromCenter(p.laneCoords, p.isDrums);
        auto fbEdge = PositionMath::getFretboardEdge(p.isDrums, adjustedPosition, p.viewportW, p.viewportH,
                                                      HIGHWAY_POS_START, p.posEnd);
        float fbWidthPx = (fbEdge.rightX - fbEdge.leftX) * FRETBOARD_SCALE;
        result.arcOffset = fbWidthPx * p.curvature * (1.0f - dist * dist);
    }

    // Final draw rect (straight path)
    result.drawRect = scaleRect(glyphRect, p.wScale, p.hScale, zOff);

    // Overlay rects
    if (p.hasOverlay)
    {
        result.overlayGlyphRect = getOverlayGlyphRect(glyphRect, p.overlayAdj);
        result.overlayDrawRect = scaleRect(result.overlayGlyphRect, p.wScale, p.hScale, zOff);
        result.overlayDrawRect.translate(p.overlayAdj.offsetX * result.overlayDrawRect.getWidth(),
                                         p.overlayAdj.offsetY * result.overlayDrawRect.getHeight());
    }

    return result;
}

juce::Rectangle<float> computeCurvedDrawRect(
    const juce::Rectangle<float>& glyphRect,
    float curvedImageAspect, float contentYOff,
    float arcOffset, float wScale, float hScale, float zOff)
{
    float curvedH = glyphRect.getWidth() / curvedImageAspect;
    float curvedY = glyphRect.getCentreY() - curvedH * 0.5f
                    + contentYOff * glyphRect.getHeight()
                    + arcOffset;
    auto curvedRect = juce::Rectangle<float>(glyphRect.getX(), curvedY,
                                              glyphRect.getWidth(), curvedH);
    return scaleRect(curvedRect, wScale, hScale, zOff);
}

// =========================================================================
// Drawing
// =========================================================================

void paintGem(juce::Graphics& g, const juce::Image& glyphImage,
              juce::Rectangle<float> destRect, float opacity)
{
    g.setOpacity(opacity);
    g.drawImage(glyphImage, destRect);
}

// =========================================================================
// Curved image cache
// =========================================================================

void clearCurvedCache()
{
    s_curvedCache.clear();
}

const CurvedImageEntry& getCurvedImage(
    juce::Image* src, int column, bool isDrums,
    float curvatureGuitar, float curvatureDrums,
    const NormalizedCoordinates* laneCoordsGuitar,
    const NormalizedCoordinates* laneCoordsDrums)
{
    if (curvatureGuitar != s_lastCachedCurvatureGuitar ||
        curvatureDrums != s_lastCachedCurvatureDrums)
    {
        s_curvedCache.clear();
        s_lastCachedCurvatureGuitar = curvatureGuitar;
        s_lastCachedCurvatureDrums = curvatureDrums;
    }

    CurveKey key{src, column, isDrums};
    auto it = s_curvedCache.find(key);
    if (it != s_curvedCache.end())
        return it->second;

    int srcW = src->getWidth() / NOTE_CACHE_DOWNSAMPLE;
    int srcH = src->getHeight() / NOTE_CACHE_DOWNSAMPLE;
    if (srcW < 1) srcW = 1;
    if (srcH < 1) srcH = 1;

    juce::Image downSrc(juce::Image::ARGB, srcW, srcH, true);
    {
        juce::Graphics gDown(downSrc);
        gDown.drawImage(*src, juce::Rectangle<float>(0.0f, 0.0f, (float)srcW, (float)srcH));
    }

    const auto& fbCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;
    float fbCenterNorm = fbCoords.normX1 + fbCoords.normWidth1 * 0.5f;
    float fbHalfWNorm = fbCoords.normWidth1 * 0.5f;

    const auto& colCoords = isDrums
        ? laneCoordsDrums[(column == 6) ? 0 : ((column < (int)DRUM_LANE_COUNT) ? column : 1)]
        : laneCoordsGuitar[(column < (int)GUITAR_LANE_COUNT) ? column : 1];

    float fbWidthInCache = (float)srcW * (fbCoords.normWidth1 / colCoords.normWidth1);
    float curv = isDrums ? curvatureDrums : curvatureGuitar;
    float arcHeight = fbWidthInCache * curv;

    float noteLeftNorm = colCoords.normX1;
    float noteRightNorm = colCoords.normX1 + colCoords.normWidth1;

    std::vector<float> colOffsets(srcW);

    for (int x = 0; x < srcW; x++)
    {
        float t = ((float)x + 0.5f) / (float)srcW;
        float xNorm = noteLeftNorm + t * (noteRightNorm - noteLeftNorm);
        float dist = (xNorm - fbCenterNorm) / fbHalfWNorm;
        colOffsets[x] = arcHeight * (1.0f - dist * dist);
    }

    float globalRef = std::min(0.0f, arcHeight);
    float maxShift = 0.0f;
    for (int x = 0; x < srcW; x++)
    {
        float shift = colOffsets[x] - globalRef;
        if (shift > maxShift) maxShift = shift;
    }

    int extraPx = (int)std::ceil(maxShift) + 2;
    int destH = srcH + extraPx;

    juce::Image dest(juce::Image::ARGB, srcW, destH, true);

    {
        juce::Image::BitmapData srcData(downSrc, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData dstData(dest, juce::Image::BitmapData::writeOnly);

        for (int x = 0; x < srcW; x++)
        {
            float yShift = colOffsets[x] - globalRef;

            for (int dy = 0; dy < destH; dy++)
            {
                float sy = (float)dy - yShift;

                int sy0 = (int)std::floor(sy);
                int sy1 = sy0 + 1;
                float frac = sy - (float)sy0;

                if (sy0 < 0 || sy1 >= srcH) {
                    if (sy0 >= 0 && sy0 < srcH) {
                        dstData.setPixelColour(x, dy, srcData.getPixelColour(x, sy0));
                    } else if (sy1 >= 0 && sy1 < srcH) {
                        dstData.setPixelColour(x, dy, srcData.getPixelColour(x, sy1));
                    }
                    continue;
                }

                auto c0 = srcData.getPixelColour(x, sy0);
                auto c1 = srcData.getPixelColour(x, sy1);
                dstData.setPixelColour(x, dy, c0.interpolatedWith(c1, frac));
            }
        }
    }

    float centerColShift = colOffsets[srcW / 2] - globalRef;
    float srcCenterInDest = centerColShift + (float)srcH * 0.5f;
    float destCenter = (float)destH * 0.5f;
    float yOffsetFraction = (srcCenterInDest - destCenter) / (float)srcH;

    auto [insertIt, _] = s_curvedCache.emplace(key, CurvedImageEntry{std::move(dest), yOffsetFraction});
    return insertIt->second;
}

// =========================================================================
// Full pipeline paintGem
// =========================================================================

void paintGem(juce::Graphics& g,
              const GemParams& params,
              juce::Image* glyphImage,
              juce::Image* overlayImage,
              float opacity,
              float curvatureGuitar,
              float curvatureDrums,
              const NormalizedCoordinates* laneCoordsGuitar,
              const NormalizedCoordinates* laneCoordsDrums)
{
    if (!glyphImage) return;

    auto rects = computeGemRects(params);

    if (params.curvature != 0.0f)
    {
        const auto& entry = getCurvedImage(glyphImage, params.gemColumn, params.isDrums,
                                           curvatureGuitar, curvatureDrums,
                                           laneCoordsGuitar, laneCoordsDrums);
        float cachedAspect = (float)entry.image.getWidth() / (float)entry.image.getHeight();
        auto curvedRect = computeCurvedDrawRect(
            rects.glyphRect, cachedAspect, entry.yOffsetFraction,
            rects.arcOffset, params.wScale, params.hScale, rects.perspZOffset);
        paintGem(g, entry.image, curvedRect, opacity);
    }
    else
    {
        paintGem(g, *glyphImage, rects.drawRect, opacity);
    }

    if (overlayImage)
    {
        if (params.curvature != 0.0f)
        {
            const auto& entry = getCurvedImage(overlayImage, params.gemColumn, params.isDrums,
                                               curvatureGuitar, curvatureDrums,
                                               laneCoordsGuitar, laneCoordsDrums);
            float cachedAspect = (float)entry.image.getWidth() / (float)entry.image.getHeight();
            auto curvedOverlayRect = computeCurvedDrawRect(
                rects.overlayGlyphRect, cachedAspect, entry.yOffsetFraction,
                rects.arcOffset, params.wScale, params.hScale, rects.perspZOffset);
            curvedOverlayRect.translate(params.overlayAdj.offsetX * curvedOverlayRect.getWidth(),
                                        params.overlayAdj.offsetY * curvedOverlayRect.getHeight());
            paintGem(g, entry.image, curvedOverlayRect, opacity);
        }
        else
        {
            paintGem(g, *overlayImage, rects.overlayDrawRect, opacity);
        }
    }
}

} // namespace NotePainter
