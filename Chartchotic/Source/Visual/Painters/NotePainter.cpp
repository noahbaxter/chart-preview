/*
    ==============================================================================

        NotePainter.cpp
        Author:  Noah Baxter

        Stateless note overlay geometry. Extracted from HighwayComponent.

    ==============================================================================
*/

#include "NotePainter.h"

using namespace PositionConstants;

namespace NotePainter
{

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

} // namespace NotePainter
