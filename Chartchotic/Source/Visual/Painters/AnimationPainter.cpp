/*
    ==============================================================================

        AnimationPainter.cpp
        Author:  Noah Baxter

        Stateless hit animation drawing. Extracted from AnimationRenderer.

    ==============================================================================
*/

#include "AnimationPainter.h"

using namespace AnimationConstants;
using namespace PositionConstants;

namespace AnimationPainter
{

void paintKick(juce::Graphics& g, const KickParams& p)
{
    if (!p.animFrame) return;

    bool isGuitar = isGuitarLike(p.activePart);
    bool isDrums = !isGuitar;
    bool useWhiteSP = p.flareImage != nullptr;

    if (PositionMath::bemaniMode)
    {
        auto kickRect = PositionMath::computeBemaniBarRect(
            isDrums, p.strikePos, p.viewportW, p.viewportH,
            p.posEnd, BAR_SIZE, p.noteAspect);

        float userScale = p.userBarScale;
        float bw = bemaniConfig.barW;
        float bh = bemaniConfig.barH;
        kickRect = kickRect.withSizeKeepingCentre(
            kickRect.getWidth() * bw * userScale * p.offset.widthScale,
            kickRect.getHeight() * bh * userScale * p.offset.heightScale
        );
        float padY = (float)p.viewportH * bemaniConfig.strikelinePos;
        kickRect.translate(p.offset.xOffset, p.offset.yOffset + (padY - kickRect.getCentreY()) + kickRect.getHeight() * bemaniConfig.hitBarNudge(isDrums));

        g.setOpacity(1.0f);
        g.drawImage(*p.animFrame, kickRect);

        if (useWhiteSP && p.anim.currentFrame <= HIT_FLARE_MAX_FRAME)
        {
            g.setOpacity(HIT_FLARE_OPACITY);
            g.drawImage(*p.flareImage, kickRect);
        }
    }
    else
    {
        auto edge = PositionMath::getColumnPosition(isDrums, p.strikePos, p.viewportW, p.viewportH,
                       HIGHWAY_POS_START, p.posEnd, p.laneCoords, BAR_SIZE, FRETBOARD_SCALE, -1);
        auto perspParams = getPerspectiveParams(isDrums);
        float colWidth = edge.rightX - edge.leftX;
        float colHeight = colWidth / perspParams.barNoteHeightRatio;
        juce::Rectangle<float> kickRect(edge.leftX, edge.centerY - colHeight * 0.5f + p.hitBarZOffset, colWidth, colHeight);

        kickRect = kickRect.withSizeKeepingCentre(
            kickRect.getWidth() * p.hitBarScale.scale * p.hitBarScale.width * p.offset.widthScale,
            kickRect.getHeight() * p.hitBarScale.scale * p.hitBarScale.height * p.offset.heightScale
        ).translated(p.offset.xOffset, p.offset.yOffset);

        g.setOpacity(1.0f);
        g.drawImage(*p.animFrame, kickRect);

        if (useWhiteSP && p.anim.currentFrame <= HIT_FLARE_MAX_FRAME)
        {
            g.setOpacity(HIT_FLARE_OPACITY);
            g.drawImage(*p.flareImage, kickRect);
        }
    }
}

void paintFret(juce::Graphics& g, const FretParams& p)
{
    bool isDrums = isDrumLike(p.activePart);

    auto edge = PositionMath::getColumnPosition(isDrums, p.strikePos, p.viewportW, p.viewportH,
                   HIGHWAY_POS_START, p.posEnd, p.laneCoords, p.sizeScale, FRETBOARD_SCALE, p.bemaniLaneIdx);
    auto perspParams = getPerspectiveParams(isDrums);
    bool barNote = isBarNote(p.anim.lane, isDrums ? Part::DRUMS : Part::GUITAR);
    float colWidth = edge.rightX - edge.leftX;
    float colHeight = colWidth / (barNote ? perspParams.barNoteHeightRatio : perspParams.regularNoteHeightRatio);

    // Arc offset to match note curvature
    float arcOffset = 0.0f;
    if (p.noteCurvature != 0.0f && !barNote)
    {
        const auto& fbCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;
        float fbCenterNorm = fbCoords.normX1 + fbCoords.normWidth1 * 0.5f;
        float fbHalfWNorm = fbCoords.normWidth1 * 0.5f;
        float colCenterNorm = p.laneCoords.normX1 + p.laneCoords.normWidth1 * 0.5f;
        float dist = (colCenterNorm - fbCenterNorm) / fbHalfWNorm;
        float fbWidthPx = colWidth * (fbCoords.normWidth1 / p.laneCoords.normWidth1);
        arcOffset = fbWidthPx * p.noteCurvature * (1.0f - dist * dist);
    }

    juce::Rectangle<float> hitRect(edge.leftX, edge.centerY - colHeight * 0.5f + p.zOffset + arcOffset, colWidth, colHeight);

    const auto& hs = barNote ? p.hitScale : p.hitScale;
    hitRect = hitRect.withSizeKeepingCentre(
        hitRect.getWidth() * hs.scale * hs.width * p.dynScale * p.offset.widthScale,
        hitRect.getHeight() * hs.scale * hs.height * p.dynScale * p.offset.heightScale
    ).translated(p.offset.xOffset, p.offset.yOffset);

    if (PositionMath::bemaniMode)
    {
        float padY = (float)p.viewportH * bemaniConfig.strikelinePos;
        float currentCenterY = hitRect.getCentreY();
        hitRect.translate(0.0f, padY - currentCenterY);
    }

    if (p.hitFrame)
    {
        g.setOpacity(HIT_FLASH_OPACITY);
        g.drawImage(*p.hitFrame, hitRect);
    }

    if (p.flareImage && p.anim.currentFrame <= HIT_FLARE_MAX_FRAME)
    {
        g.setOpacity(HIT_FLARE_OPACITY);
        g.drawImage(*p.flareImage, hitRect);
    }
}

} // namespace AnimationPainter
