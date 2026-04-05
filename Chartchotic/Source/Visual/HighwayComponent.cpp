/*
    ==============================================================================

        HighwayComponent.cpp
        Author: Noah Baxter

    ==============================================================================
*/

#include "HighwayComponent.h"
#include "TrackImageCache.h"
#include "../UI/ControlConstants.h"
#include "../Midi/Providers/MidiWriter.h"
#include "../UI/Theme.h"
#include "Painters/LanePainter.h"

HighwayComponent::HighwayComponent(juce::ValueTree& state, AssetManager& assetManager)
    : state(state),
      assetManager(assetManager),
      sceneRenderer(state, assetManager),
      trackRenderer(state)
{
    // Sync activePart from state
    setActivePart(getPartFromState(state));
    setWantsKeyboardFocus(true);
#ifdef DEBUG
    debugColour = juce::Colour::fromHSV(
        juce::Random::getSystemRandom().nextFloat(), 0.4f, 0.4f, 1.0f);
#endif
}

void HighwayComponent::setActivePart(Part part)
{
    pendingPart = part;
    // Defer renderer update until matching frame data arrives (avoids
    // wrong-color flash where activePart changes but frameData is stale)
}

void HighwayComponent::commitPendingPart()
{
    if (activePart == pendingPart) return;
    activePart = pendingPart;
    sceneRenderer.activePart = pendingPart;
    trackRenderer.activePart = pendingPart;
    rebuildTrack();
}

void HighwayComponent::paint(juce::Graphics& g)
{
#ifdef DEBUG
    if (showDebugColour && debugColour != juce::Colour()) g.fillAll(debugColour);
    ScopedPhaseMeasure _hwPaintMeasure(debugHighwayPaint_us, sceneRenderer.collectPhaseTiming);
#endif

    // During resize debounce, render at baked dimensions and scale to fit.
    // During highway-length debounce (same dimensions), fill+fade is fresh
    // but layer overlays and texture use stale baked images.
    bool debouncing = isTimerRunning() && bakedRenderW > 0 && bakedRenderH > 0;

    int w        = debouncing ? bakedRenderW  : renderWidth;
    int h        = debouncing ? bakedRenderH  : renderHeight;
    int overflow = debouncing ? bakedOverflow  : topOverflow;
    int totalH   = h + overflow;

    if (stretchToFill && !PositionMath::bemaniMode)
    {
        // Non-uniform stretch to fill all available space (perspective only)
        float sx = (float)getWidth()  / (float)w;
        float sy = (float)getHeight() / (float)totalH;
        g.addTransform(juce::AffineTransform::scale(sx, sy));
    }
    else
    {
        // Uniform scale to maximize height, centered horizontally, bottom-anchored.
        // Bemani always uses uniform scale to avoid aspect ratio distortion.
        float scale = std::min((float)getWidth() / (float)w,
                               (float)getHeight() / (float)totalH);
        float scaledW = (float)w * scale;
        float scaledH = (float)totalH * scale;
        float offsetX = ((float)getWidth() - scaledW) / 2.0f;
        float offsetY = (float)getHeight() - scaledH;
        g.addTransform(juce::AffineTransform(scale, 0.0f, offsetX, 0.0f, scale, offsetY));
    }

    // Track fill, layers, and texture are baked at totalH (viewport + overflow).
    // Draw them in component coordinates (no translation needed).
    if (showHighway)
    {
        bool useCache = trackImageCache && !PositionMath::bemaniMode;
#ifdef DEBUG
        {
            ScopedPhaseMeasure _trackMeasure(debugTrackRender_us, sceneRenderer.collectPhaseTiming);
            if (useCache)
            {
                auto& cached = trackImageCache->get(isDrumLike(activePart));
                if (cached.valid)
                    trackRenderer.paintFromCache(g, cached.fadedTrack, w, totalH);
            }
            else
                trackRenderer.paint(g, w, totalH);
        }
#else
        if (useCache)
        {
            auto& cached = trackImageCache->get(isDrumLike(activePart));
            if (cached.valid)
                trackRenderer.paintFromCache(g, cached.fadedTrack, w, totalH);
        }
        else
            trackRenderer.paint(g, w, totalH);
#endif
        trackRenderer.paintTexture(g, frameData.scrollOffset, w, totalH);
    }
    // Bemani overlay (lane dividers, strikeline) always draws — independent of highway texture toggle
    if (showHighway || PositionMath::bemaniMode)
        trackRenderer.paintBemaniOverlay(g, w, totalH);

    // Translate so scene renderer (notes, gridlines, etc.) sees viewport coordinates.
    // Overlay images are drawn at (0, -overlayYOffset) to extend into the overflow area.
    if (overflow > 0)
        g.addTransform(juce::AffineTransform::translation(0.0f, (float)overflow));

    sceneRenderer.paint(g, w, h,
                        frameData.trackWindow, frameData.sustainWindow, frameData.gridlines,
                        frameData.flipRegions, frameData.eventMarkers,
                        frameData.windowStartTime, frameData.windowEndTime, frameData.isPlaying);

    // Bemani sidebar masks — drawn after notes/sustains to clip overflow.
    // Must cover the full component height. In Bemani mode overflow=0 so h=totalH.
    if (PositionMath::bemaniMode)
    {
        // Undo the overflow translation to draw in component coordinates
        if (overflow > 0)
            g.addTransform(juce::AffineTransform::translation(0.0f, -(float)overflow));
        trackRenderer.paintBemaniSidebars(g, w, totalH);
        if (overflow > 0)
            g.addTransform(juce::AffineTransform::translation(0.0f, (float)overflow));
    }

    // Disco ball indicator when disco flip is active at current playhead
    if (frameData.discoFlipActive)
    {
        auto* discoBall = assetManager.getDiscoBallImage();
        if (discoBall != nullptr && discoBall->isValid())
        {
            int size = juce::jmax(32, w / 8);
            int margin = size / 4;
            juce::Rectangle<float> dest((float)(w - size - margin), (float)margin,
                                         (float)size, (float)size);
            g.setOpacity(0.7f);
            g.drawImage(*discoBall, dest);
            g.setOpacity(1.0f);
        }
    }
}

void HighwayComponent::paintOverChildren(juce::Graphics& g)
{
    if (showPartLabel)
    {
        auto iconData = getPartIcon(activePart);
        juce::String label = getPartDisplayName(activePart);

        juce::Image icon;
        if (iconData.data != nullptr)
            icon = juce::ImageCache::getFromMemory(iconData.data, iconData.size);
        bool hasIcon = icon.isValid();

        float s = (float)getWidth() / 600.0f;
        int iconSize = hasIcon ? juce::roundToInt(labelIconSize * s) : 0;
        int pad = juce::roundToInt(10.0f * s);
        float fontSize = 18.0f * s;
        auto font = Theme::getUIFont(fontSize);
        int textW = (int)font.getStringWidthFloat(label);
        int totalW = (hasIcon ? iconSize + pad : 0) + textW + pad * 2;
        int totalH = juce::jmax(iconSize, juce::roundToInt(fontSize * 1.5f)) + pad * 2;

        int x = (getWidth() - totalW) / 2;
        int y = getHeight() - totalH - pad;

        auto pillBounds = juce::Rectangle<float>((float)x, (float)y, (float)totalW, (float)totalH);
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRoundedRectangle(pillBounds, 6.0f * s);

        float textX = pillBounds.getX() + pad;

        if (hasIcon)
        {
            auto iconBounds = juce::Rectangle<float>(
                pillBounds.getX() + pad, pillBounds.getCentreY() - iconSize * 0.5f,
                (float)iconSize, (float)iconSize);
            g.setOpacity(0.9f);
            g.drawImage(icon, iconBounds);
            g.setOpacity(1.0f);
            textX += iconSize + pad * 0.5f;
        }

        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.setFont(font);
        auto textBounds = juce::Rectangle<float>(
            textX, pillBounds.getY(),
            pillBounds.getRight() - textX - pad,
            (float)totalH);
        g.drawText(label, textBounds, hasIcon ? juce::Justification::centredLeft : juce::Justification::centred);
    }

    if (showDifficultyLabel)
    {
        juce::uint32 badgeColor;
        juce::String diffName;
        switch (displaySkillLevel)
        {
            case SkillLevel::EXPERT: badgeColor = Theme::red;    diffName = "Expert"; break;
            case SkillLevel::HARD:   badgeColor = Theme::orange;  diffName = "Hard"; break;
            case SkillLevel::MEDIUM: badgeColor = Theme::yellow;  diffName = "Medium"; break;
            case SkillLevel::EASY:   badgeColor = Theme::green;   diffName = "Easy"; break;
            default:                 badgeColor = Theme::textDim; diffName = "?"; break;
        }

        float s = (float)getWidth() / 600.0f;
        int circleSize = juce::roundToInt(labelIconSize * s);
        int pad = juce::roundToInt(10.0f * s);
        float fontSize = 18.0f * s;
        auto font = Theme::getUIFont(fontSize);
        int textW = (int)font.getStringWidthFloat(diffName);
        int totalW = circleSize + pad + textW + pad * 2;
        int totalH = juce::jmax(circleSize, juce::roundToInt(fontSize * 1.5f)) + pad * 2;

        int x = (getWidth() - totalW) / 2;
        int y = getHeight() - totalH - pad;

        auto pillBounds = juce::Rectangle<float>((float)x, (float)y, (float)totalW, (float)totalH);
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRoundedRectangle(pillBounds, 6.0f * s);

        // Colored circle
        auto circleBounds = juce::Rectangle<float>(
            pillBounds.getX() + pad, pillBounds.getCentreY() - circleSize * 0.5f,
            (float)circleSize, (float)circleSize);
        g.setColour(juce::Colour(badgeColor));
        g.fillEllipse(circleBounds);

        // Difficulty letter inside circle
        juce::String letter;
        switch (displaySkillLevel)
        {
            case SkillLevel::EXPERT: letter = "X"; break;
            case SkillLevel::HARD:   letter = "H"; break;
            case SkillLevel::MEDIUM: letter = "M"; break;
            case SkillLevel::EASY:   letter = "E"; break;
            default:                 letter = "?"; break;
        }
        g.setColour(juce::Colours::white);
        g.setFont(Theme::getUIFont(20.0f * s));
        g.drawText(letter, circleBounds.toNearestInt(), juce::Justification::centred);

        // Difficulty name text
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.setFont(font);
        float textX = circleBounds.getRight() + pad * 0.5f;
        auto textBounds = juce::Rectangle<float>(
            textX, pillBounds.getY(),
            pillBounds.getRight() - textX - pad,
            (float)totalH);
        g.drawText(diffName, textBounds, juce::Justification::centredLeft);
    }

    // Sustain drag preview
    if (writeMode && isDragging && dragIsLeftButton && dragStartResult.valid && hoverValid && !frameData.isPlaying)
    {
        int dragLane = hoverResult.valid ? hoverResult.laneIndex : dragStartResult.laneIndex;

        // Snap start position to grid if snap is enabled
        float startPos = drawModeSnapEnabled
            ? snapToNearestGridline(dragStartResult.normalizedPosition)
            : dragStartResult.normalizedPosition;

        auto headOv = computeNoteOverlay(startPos, dragLane);

        // Draw sustain tail using LanePainter if cursor is forward in time
        float hoverPos = drawModeSnapEnabled
            ? snapToNearestGridline(hoverResult.normalizedPosition)
            : hoverResult.normalizedPosition;

        if (hoverPos > startPos && dragLane >= 0)
        {
            using namespace PositionConstants;
            bool isDrums = isDrumLike(activePart);

            // Resolve lane coordinates and column for LanePainter
            uint gemCol;
            NormalizedCoordinates laneCoords;
            if (isDrums) {
                uint dIdx = (dragLane == 0) ? 0 : (uint)dragLane;
                gemCol = (dragLane == 0) ? 0 : (uint)dragLane;
                laneCoords = drumBezierLaneCoords[dIdx];
            } else {
                gemCol = (uint)dragLane;
                laneCoords = guitarBezierLaneCoords[gemCol];
            }

            bool isBar = isBarNote(gemCol, activePart);
            float sustW = isBar ? SUSTAIN_OPEN_WIDTH : SUSTAIN_WIDTH;
            float laneScale = isBar ? BAR_SIZE : GEM_SIZE;
            auto colour = assetManager.getLaneColour(gemCol, activePart, false);

            LanePainter::Params lp {
                gemCol, activePart,
                startPos, hoverPos,
                0.45f, sustW, colour, false,
                (uint)renderWidth, (uint)renderHeight,
                sceneRenderer.highwayPosEnd,
                laneCoords, laneScale, -1,
                {},
                sceneRenderer.farFadeEnd, sceneRenderer.farFadeLen, sceneRenderer.farFadeCurve
            };

            // LanePainter works in render space — replicate paint()'s full transform:
            // 1. Scale to fit component  2. Translate by overflow for scene content
            int w = renderWidth, totalH = renderHeight + topOverflow;
            g.saveState();
            if (stretchToFill && !PositionMath::bemaniMode)
            {
                float sx = (float)getWidth() / (float)w;
                float sy = (float)getHeight() / (float)totalH;
                g.addTransform(juce::AffineTransform::scale(sx, sy));
            }
            else
            {
                float s = std::min((float)getWidth() / (float)w, (float)getHeight() / (float)totalH);
                float ox = ((float)getWidth() - (float)w * s) / 2.0f;
                float oy = (float)getHeight() - (float)totalH * s;
                g.addTransform(juce::AffineTransform(s, 0.0f, ox, 0.0f, s, oy));
            }
            // Scene content offset (notes/sustains/gridlines draw below overflow area)
            if (topOverflow > 0)
                g.addTransform(juce::AffineTransform::translation(0.0f, (float)topOverflow));
            LanePainter::paint(g, lp);
            g.restoreState();
        }

        // Draw note head gem image
        if (dragLane >= 0)
        {
            using namespace PositionConstants;
            bool isDrums = isDrumLike(activePart);

            uint gemCol;
            NormalizedCoordinates laneCoords;
            if (isDrums) {
                uint dIdx = (dragLane == 0) ? 0 : (uint)dragLane;
                gemCol = (dragLane == 0) ? 0 : (uint)dragLane;
                laneCoords = drumBezierLaneCoords[dIdx];
            } else {
                gemCol = (uint)dragLane;
                laneCoords = guitarBezierLaneCoords[gemCol];
            }

            bool isBar = isBarNote(gemCol, activePart);
            GemWrapper defaultGem(Gem::NOTE, false);
            juce::Image* glyphImage = isGuitarLike(activePart)
                ? assetManager.getGuitarGlyphImage(defaultGem, gemCol, false)
                : assetManager.getDrumGlyphImage(defaultGem, gemCol, false);

            if (glyphImage != nullptr)
            {
                float noteCurv = isDrums ? sceneRenderer.noteCurvatureDrums
                                         : sceneRenderer.noteCurvatureGuitar;
                float curvature = isBar ? BAR_CURVATURE : noteCurv;

                // Foreshorten
                float foreshorten = 1.0f;
                if (!PositionMath::bemaniMode)
                {
                    float adjustedPos = isBar ? startPos + BAR_NOTE_POS_OFFSET : startPos;
#ifdef DEBUG
                    const auto& pp = PositionMath::perspParams(isDrums);
#else
                    auto pp = getPerspectiveParams(isDrums);
#endif
                    float depth = std::max(0.0f, adjustedPos) / pp.vanishingPointDepth;
                    float scaleNear = 1.0f + (pp.highwayDepth / pp.playerDistance) * pp.perspectiveStrength;
                    float psCur = scaleNear / (1.0f + depth * (scaleNear - 1.0f));
                    float rawRatio = psCur / scaleNear;
                    foreshorten = 1.0f - (1.0f - rawRatio) * NOTE_DEPTH_FORESHORTEN;
                }

                // Strike width for perspective Z scaling
                float rawZOff = isBar ? sceneRenderer.getBarZOffset() : sceneRenderer.getGemZOffset();
                float strikeWidth = 1.0f;
                if (!PositionMath::bemaniMode)
                {
                    if (isBar)
                    {
                        auto fbStrike = PositionMath::getFretboardEdge(isDrums, 0.0f,
                            (uint)renderWidth, (uint)renderHeight,
                            HIGHWAY_POS_START, sceneRenderer.highwayPosEnd);
                        strikeWidth = (fbStrike.rightX - fbStrike.leftX) * BAR_FRETBOARD_FIT * BAR_SIZE;
                    }
                    else
                    {
                        auto strikeEdge = PositionMath::getColumnPosition(isDrums, 0.0f,
                            (uint)renderWidth, (uint)renderHeight,
                            HIGHWAY_POS_START, sceneRenderer.highwayPosEnd,
                            laneCoords, GEM_SIZE, FRETBOARD_SCALE, -1);
                        strikeWidth = strikeEdge.rightX - strikeEdge.leftX;
                    }
                }

                NotePainter::GemParams gp;
                gp.position = startPos;
                gp.gemColumn = (int)gemCol;
                gp.isBar = isBar;
                gp.isDrums = isDrums;
                gp.viewportW = (uint)renderWidth;
                gp.viewportH = (uint)renderHeight;
                gp.posEnd = sceneRenderer.highwayPosEnd;
                gp.imageAspect = (float)glyphImage->getWidth() / (float)glyphImage->getHeight();
                gp.sizeScale = isBar ? BAR_SIZE : GEM_SIZE;
                gp.userScale = 1.0f;
                gp.wScale = 1.0f;
                gp.hScale = 1.0f;
                gp.foreshorten = foreshorten;
                gp.rawZOffset = rawZOff;
                gp.strikeWidth = strikeWidth;
                gp.curvature = curvature;
                gp.laneCoords = laneCoords;
                gp.bemaniLaneIdx = -1;
                gp.bemaniNudgeY = 0.0f;
                gp.hasOverlay = false;
                gp.overlayAdj = {};

                // Render in render-space transform
                int w = renderWidth, totalH = renderHeight + topOverflow;
                g.saveState();
                if (stretchToFill && !PositionMath::bemaniMode)
                {
                    float sx = (float)getWidth() / (float)w;
                    float sy = (float)getHeight() / (float)totalH;
                    g.addTransform(juce::AffineTransform::scale(sx, sy));
                }
                else
                {
                    float s = std::min((float)getWidth() / (float)w, (float)getHeight() / (float)totalH);
                    float ox = ((float)getWidth() - (float)w * s) / 2.0f;
                    float oy = (float)getHeight() - (float)totalH * s;
                    g.addTransform(juce::AffineTransform(s, 0.0f, ox, 0.0f, s, oy));
                }
                if (topOverflow > 0)
                    g.addTransform(juce::AffineTransform::translation(0.0f, (float)topOverflow));

                NotePainter::paintGem(g, gp, glyphImage, nullptr, 0.75f,
                                      sceneRenderer.noteCurvatureGuitar, sceneRenderer.noteCurvatureDrums,
                                      sceneRenderer.guitarLaneCoordsLocal, sceneRenderer.drumLaneCoordsLocal);
                g.restoreState();
            }
        }
    }
    // Write mode hover cursor (disabled during playback and drag)
    else if (writeMode && hoverValid && !isDragging && !frameData.isPlaying)
    {
        // Snap cursor to grid in draw mode
        float ghostPos = (isDrawMode && drawModeSnapEnabled)
            ? snapToNearestGridline(hoverResult.normalizedPosition)
            : hoverResult.normalizedPosition;
        auto ov = computeNoteOverlay(ghostPos, hoverResult.laneIndex);
        auto ghostPath = buildCurvedNotePath(ov);

        if (hoverOnExistingNote)
        {
            g.setColour(juce::Colour(0x30ff4444));
            g.fillPath(ghostPath);
            g.setColour(juce::Colour(0xccff6666));
            g.strokePath(ghostPath, juce::PathStrokeType(2.0f));
        }
        else
        {
            g.setColour(juce::Colours::white.withAlpha(0.25f));
            g.fillPath(ghostPath);
            g.setColour(juce::Colours::white.withAlpha(0.7f));
            g.strokePath(ghostPath, juce::PathStrokeType(1.5f));
        }

        // Time line across fretboard — uses the ghost's Y as the baseline
        // so it always aligns exactly with where the note will be placed
        {
            using namespace PositionConstants;
            bool isDrums = isDrumLike(activePart);
            int w = renderWidth, totalH = renderHeight + topOverflow;
            float sx, sy2, ox, oy;
            if (stretchToFill && !PositionMath::bemaniMode)
            { sx = (float)getWidth() / (float)w; sy2 = (float)getHeight() / (float)totalH; ox = 0; oy = 0; }
            else
            { float s = std::min((float)getWidth() / (float)w, (float)getHeight() / (float)totalH);
              sx = s; sy2 = s; ox = ((float)getWidth() - (float)w * s) / 2.0f; oy = (float)getHeight() - (float)totalH * s; }

            auto fbEdge = PositionMath::getFretboardEdge(
                isDrums, ov.position, (uint)renderWidth, (uint)renderHeight,
                HIGHWAY_POS_START, HIGHWAY_POS_END);
            float fbScreenLeft  = fbEdge.leftX * sx + ox;
            float fbScreenRight = fbEdge.rightX * sx + ox;
            float fbScreenMid   = (fbScreenLeft + fbScreenRight) * 0.5f;

            // Nudge line toward strikeline to align with note visual center
            float lineNudge = ov.screenH * 0.35f;
            float lineY = ov.screenCenterY + lineNudge;

            if (std::abs(ov.curvature) > 0.001f)
            {
                float fbWidthPx = (fbEdge.rightX - fbEdge.leftX) * FRETBOARD_SCALE;
                const auto& fbCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;
                float fbCenterNorm = fbCoords.normX1 + fbCoords.normWidth1 * 0.5f;
                float fbHalfWNorm = fbCoords.normWidth1 * 0.5f;

                float pts[3] = { fbEdge.leftX, (fbEdge.leftX + fbEdge.rightX) * 0.5f, fbEdge.rightX };
                float yOff[3];
                for (int i = 0; i < 3; i++)
                {
                    float normX = pts[i] / (float)renderWidth;
                    float d = (normX - fbCenterNorm) / fbHalfWNorm;
                    yOff[i] = fbWidthPx * ov.curvature * (1.0f - d * d) * ov.sy;
                }

                juce::Path linePath;
                linePath.startNewSubPath(fbScreenLeft, lineY + yOff[0]);
                linePath.quadraticTo(fbScreenMid, lineY + yOff[1],
                                     fbScreenRight, lineY + yOff[2]);

                g.setColour(juce::Colours::white.withAlpha(0.12f));
                g.strokePath(linePath, juce::PathStrokeType(5.0f));
                g.setColour(juce::Colours::white.withAlpha(0.5f));
                g.strokePath(linePath, juce::PathStrokeType(1.5f));
            }
            else
            {
                g.setColour(juce::Colours::white.withAlpha(0.12f));
                g.drawLine(fbScreenLeft, lineY, fbScreenRight, lineY, 5.0f);
                g.setColour(juce::Colours::white.withAlpha(0.5f));
                g.drawLine(fbScreenLeft, lineY, fbScreenRight, lineY, 1.5f);
            }
        }
    }

    // Selection highlight (disabled during playback)
    // selectedTime is set each frame by PluginEditor from PPQ (scroll-stable)
    if (writeMode && hasSelection && selectedLane >= 0 && !frameData.isPlaying)
    {
        double windowTimeSpan = frameData.windowEndTime - frameData.windowStartTime;
        float selPos = (windowTimeSpan > 0.0)
            ? (float)((selectedTime - frameData.windowStartTime) / windowTimeSpan)
            : 0.0f;

        // Only draw if the note exists in the current frame and is visible
        double foundTime; int foundLane;
        if (selPos >= PositionConstants::HIGHWAY_POS_START && selPos <= sceneRenderer.farFadeEnd
            && findNoteAtPosition(selPos, selectedLane, foundTime, foundLane))
        {
            float foundPos = (windowTimeSpan > 0.0)
                ? (float)((foundTime - frameData.windowStartTime) / windowTimeSpan)
                : selPos;

            auto ov = computeNoteOverlay(foundPos, selectedLane);
            auto selPath = buildCurvedNotePath(ov, 2.0f);

            g.setColour(juce::Colour(0x4000ddff));
            g.fillPath(selPath);
            g.setColour(juce::Colour(0xff00ddff));
            g.strokePath(selPath, juce::PathStrokeType(2.0f));
        }
    }
}

void HighwayComponent::resized()
{
    if (PositionMath::bemaniMode)
    {
        // No baked assets in Bemani mode — rebuild immediately, no debounce
        rebuildTrack();
    }
    else
    {
        startTimer(rebuildDebounceMs);
    }
}

void HighwayComponent::timerCallback()
{
    stopTimer();
    rebuildTrack();
}

void HighwayComponent::setFrameData(const HighwayFrameData& data)
{
    // Clear selection when playback starts (selectedTime would be stale)
    if (data.isPlaying && !frameData.isPlaying)
        hasSelection = false;

    frameData = data;
    if (frameData.builtForPart == pendingPart)
        commitPendingPart();
}

void HighwayComponent::updateOverflow()
{
    if (renderWidth <= 0 || renderHeight <= 0) return;
    if (PositionMath::bemaniMode) { topOverflow = 0; return; }
    bool isDrums = isDrumLike(activePart);
    auto farEdge = PositionMath::getFretboardEdge(
        isDrums, sceneRenderer.farFadeEnd, renderWidth, renderHeight,
        PositionConstants::HIGHWAY_POS_START, sceneRenderer.highwayPosEnd);
    topOverflow = std::max(0, (int)std::ceil(-farEdge.centerY));
}

void HighwayComponent::rebuildTrack()
{
    int w = renderWidth;
    int h = renderHeight;
    if (w <= 0 || h <= 0) return;

    // Recompute overflow in case perspective params changed
    int prevOverflow = topOverflow;
    updateOverflow();

    bool isDrums = isDrumLike(activePart);
    bool useCache = trackImageCache && trackImageCache->isValid() && !PositionMath::bemaniMode;

    sceneRenderer.rescaleAssets(w);
    sceneRenderer.overlayYOffset = topOverflow;

    // Pass lane coords to TrackRenderer for perspective-projected lane lines
    trackRenderer.setLaneCoords(
        isDrums ? sceneRenderer.drumLaneCoordsLocal : sceneRenderer.guitarLaneCoordsLocal,
        isDrums ? (int)PositionConstants::DRUM_LANE_COUNT : (int)PositionConstants::GUITAR_LANE_COUNT);

    if (useCache)
    {
        // With a valid shared cache: skip all image baking.
        // Rebuild geometry + texture prebake when dimensions or instrument changed
        // (texture scanline LUT depends on fretboard edges which differ guitar vs drums).
        bool dimsChanged = w != bakedRenderW || h != bakedRenderH || topOverflow != bakedOverflow;
        bool partChanged = isDrums != trackRenderer.getCachedIsDrums();
        if (dimsChanged || partChanged)
            trackRenderer.rebuild(w, h, topOverflow,
                                  sceneRenderer.farFadeEnd, sceneRenderer.farFadeLen, sceneRenderer.farFadeCurve,
                                  sceneRenderer.highwayPosEnd, /*geometryOnly=*/true);

        // Point overlays at the correct cache entry for this instrument
        sceneRenderer.clearOverlays();
        auto& cached = trackImageCache->get(isDrums);
        if (cached.valid)
        {
            sceneRenderer.setOverlay(DrawOrder::TRACK_STRIKELINE, &cached.layers[TrackRenderer::STRIKELINE]);
            sceneRenderer.setOverlay(DrawOrder::TRACK_LANE_LINES, &cached.layers[TrackRenderer::LANE_LINES]);
            sceneRenderer.setOverlay(DrawOrder::TRACK_SIDEBARS,   &cached.layers[TrackRenderer::SIDEBARS]);
            sceneRenderer.setOverlay(DrawOrder::TRACK_CONNECTORS, &cached.layers[TrackRenderer::CONNECTORS]);
        }
    }
    else
    {
        // No cache (standalone, first frame, or Bemani mode) — full rebuild
        trackRenderer.rebuild(w, h, topOverflow,
                              sceneRenderer.farFadeEnd, sceneRenderer.farFadeLen, sceneRenderer.farFadeCurve,
                              sceneRenderer.highwayPosEnd);

        // Always reset overlays — Bemani mode uses programmatic drawing, perspective uses baked images
        sceneRenderer.clearOverlays();
        if (PositionMath::bemaniMode)
        {
            int rw = w, rh = h + topOverflow;
            sceneRenderer.setCustomDrawCall(DrawOrder::TRACK_SIDEBARS,
                [this, rw, rh](juce::Graphics& g) { trackRenderer.paintBemaniRails(g, rw, rh); });
        }
        else
        {
            sceneRenderer.setOverlay(DrawOrder::TRACK_STRIKELINE, &trackRenderer.getLayerImage(TrackRenderer::STRIKELINE));
            sceneRenderer.setOverlay(DrawOrder::TRACK_LANE_LINES, &trackRenderer.getLayerImage(TrackRenderer::LANE_LINES));
            sceneRenderer.setOverlay(DrawOrder::TRACK_SIDEBARS,   &trackRenderer.getLayerImage(TrackRenderer::SIDEBARS));
            sceneRenderer.setOverlay(DrawOrder::TRACK_CONNECTORS, &trackRenderer.getLayerImage(TrackRenderer::CONNECTORS));
        }
    }

    bakedRenderW = w;
    bakedRenderH = h;
    bakedOverflow = topOverflow;

    if (topOverflow != prevOverflow && onOverflowChanged) onOverflowChanged();
}

void HighwayComponent::onInstrumentChanged()
{
    setActivePart(getPartFromState(state));

    if (trackImageCache && trackImageCache->isValid() && !PositionMath::bemaniMode)
    {
        // Cache active — just swap overlay pointers, no rebuild needed
        bool isDrums = isDrumLike(activePart);
        sceneRenderer.clearOverlays();
        auto& cached = trackImageCache->get(isDrums);
        if (cached.valid)
        {
            sceneRenderer.setOverlay(DrawOrder::TRACK_STRIKELINE, &cached.layers[TrackRenderer::STRIKELINE]);
            sceneRenderer.setOverlay(DrawOrder::TRACK_LANE_LINES, &cached.layers[TrackRenderer::LANE_LINES]);
            sceneRenderer.setOverlay(DrawOrder::TRACK_SIDEBARS,   &cached.layers[TrackRenderer::SIDEBARS]);
            sceneRenderer.setOverlay(DrawOrder::TRACK_CONNECTORS, &cached.layers[TrackRenderer::CONNECTORS]);
        }
        repaint();
    }
    else
    {
        // No cache — full rebuild path
        trackRenderer.invalidate();
        rebuildTrack();
        repaint();
    }
}

// =============================================================================
// Write Mode
// =============================================================================

NotePainter::NoteRect HighwayComponent::computeNoteOverlay(float position, int lane) const
{
    using namespace PositionConstants;
    bool isDrums = isDrumLike(activePart);

    // Compute foreshortening
    float foreshorten = 1.0f;
    if (!PositionMath::bemaniMode)
    {
        bool isBar = (lane == 0);
#ifdef DEBUG
        const auto& pp = PositionMath::perspParams(isDrums);
#else
        auto pp = getPerspectiveParams(isDrums);
#endif
        float depthPos = isBar ? position + BAR_NOTE_POS_OFFSET : position;
        float depth = std::max(0.0f, depthPos) / pp.vanishingPointDepth;
        float scaleNear = 1.0f + (pp.highwayDepth / pp.playerDistance) * pp.perspectiveStrength;
        float psCur = scaleNear / (1.0f + depth * (scaleNear - 1.0f));
        float rawRatio = psCur / scaleNear;
        foreshorten = 1.0f - (1.0f - rawRatio) * NOTE_DEPTH_FORESHORTEN;
    }

    float curvature = isDrums ? sceneRenderer.noteCurvatureDrums
                              : sceneRenderer.noteCurvatureGuitar;

    return NotePainter::computeRect(position, lane, activePart,
                                    renderWidth, renderHeight, HIGHWAY_POS_END, topOverflow,
                                    stretchToFill, getWidth(), getHeight(),
                                    foreshorten, sceneRenderer.getGemZOffset(), sceneRenderer.getBarZOffset(),
                                    curvature);
}

juce::Path HighwayComponent::buildCurvedNotePath(const NotePainter::NoteRect& nr, float expand) const
{
    return NotePainter::buildCurvedPath(nr, activePart, renderWidth, renderHeight, expand);
}

float HighwayComponent::snapToNearestGridline(float normalizedPos) const
{
    double windowTimeSpan = frameData.windowEndTime - frameData.windowStartTime;
    if (windowTimeSpan <= 0.0 || frameData.gridlines.empty())
        return normalizedPos;

    float bestPos = normalizedPos;
    float bestDist = std::numeric_limits<float>::max();

    for (const auto& gl : frameData.gridlines)
    {
        float glPos = (float)((gl.time - frameData.windowStartTime) / windowTimeSpan);
        float dist = std::abs(glPos - normalizedPos);
        if (dist < bestDist)
        {
            bestDist = dist;
            bestPos = glPos;
        }
    }
    return bestPos;
}

void HighwayComponent::setWriteMode(bool on, MidiWriter* writer, int trackIndex)
{
    writeMode = on;
    midiWriter = on ? writer : nullptr;
    writeTrackIndex = on ? trackIndex : -1;
    sceneRenderer.writeMode = on;
    if (!on) clearSelection();
}

void HighwayComponent::setSelection(double timeFromCursor, int lane)
{
    hasSelection = true;
    selectedTime = timeFromCursor;
    selectedLane = lane;
    repaint();
}

void HighwayComponent::clearSelection()
{
    if (hasSelection)
    {
        hasSelection = false;
        selectedTime = 0.0;
        selectedLane = -1;
        repaint();
    }
}

juce::Point<float> HighwayComponent::screenToRenderCoords(juce::Point<float> screen) const
{
    int w = renderWidth;
    int h = renderHeight;
    int overflow = topOverflow;
    int totalH = h + overflow;

    if (w <= 0 || totalH <= 0)
        return screen;

    if (stretchToFill && !PositionMath::bemaniMode)
    {
        float sx = (float)getWidth() / (float)w;
        float sy = (float)getHeight() / (float)totalH;
        return { screen.x / sx, screen.y / sy };
    }
    else
    {
        float scale = std::min((float)getWidth() / (float)w,
                               (float)getHeight() / (float)totalH);
        float scaledW = (float)w * scale;
        float scaledH = (float)totalH * scale;
        float offsetX = ((float)getWidth() - scaledW) / 2.0f;
        float offsetY = (float)getHeight() - scaledH;
        return { (screen.x - offsetX) / scale, (screen.y - offsetY) / scale };
    }
}

HitTestResult HighwayComponent::performHitTest(juce::Point<float> screenPos) const
{
    auto renderPt = screenToRenderCoords(screenPos);

    // screenToRenderCoords maps to (0,0)-(w, totalH) render space.
    // PositionMath works in (0,0)-(w, renderHeight), with overflow above.
    // Subtract topOverflow to get into PositionMath's coordinate system.
    float hitY = renderPt.y - (float)topOverflow;

    bool isDrums = isDrumLike(activePart);

    return hitTestMapper.hitTest(
        renderPt.x, hitY,
        (uint)renderWidth, (uint)renderHeight,
        frameData.windowStartTime, frameData.windowEndTime,
        isDrums, sceneRenderer.farFadeEnd);
}

bool HighwayComponent::findNoteAtPosition(float normalizedPosition, int laneIndex,
                                           double& outTime, int& outLane) const
{
    if (laneIndex < 0 || laneIndex >= (int)LANE_COUNT)
        return false;

    double windowTimeSpan = frameData.windowEndTime - frameData.windowStartTime;
    if (windowTimeSpan <= 0.0)
        return false;

    // Convert normalizedPosition back to time
    double targetTime = (double)normalizedPosition * windowTimeSpan + frameData.windowStartTime;

    // Find the closest trackWindow entry with a note in this lane
    double bestDist = std::numeric_limits<double>::max();
    double bestTime = 0.0;
    bool found = false;

    for (const auto& [time, frame] : frameData.trackWindow)
    {
        if (frame[(size_t)laneIndex].gem == Gem::NONE)
            continue;

        double dist = std::abs(time - targetTime);
        if (dist < bestDist)
        {
            bestDist = dist;
            bestTime = time;
            found = true;
        }
    }

    if (!found)
        return false;

    // Tolerance: the note's normalized position must be within ~5% of highway length
    double bestNormPos = (bestTime - frameData.windowStartTime) / windowTimeSpan;
    if (std::abs(bestNormPos - (double)normalizedPosition) > 0.05)
        return false;

    outTime = bestTime;
    outLane = laneIndex;
    return true;
}

void HighwayComponent::mouseMove(const juce::MouseEvent& event)
{
    if (!writeMode || frameData.isPlaying)
        return;

    hoverResult = performHitTest(event.position);
    hoverValid = hoverResult.valid && hoverResult.laneIndex >= 0;

    // Check if hovering over an existing note
    hoverOnExistingNote = false;
    if (hoverValid)
    {
        double t; int l;
        hoverOnExistingNote = findNoteAtPosition(hoverResult.normalizedPosition,
                                                  hoverResult.laneIndex, t, l);
    }

    repaint();
}

void HighwayComponent::mouseExit(const juce::MouseEvent&)
{
    if (writeMode && (hoverValid || hasSelection))
    {
        hoverValid = false;
        hoverOnExistingNote = false;
        repaint();
    }
}

void HighwayComponent::mouseDown(const juce::MouseEvent& event)
{
    if (!writeMode || frameData.isPlaying)
        return;

    grabKeyboardFocus();

    // Store for click-vs-drag disambiguation
    mouseDownScreenPos = event.position;
    dragStartResult = performHitTest(event.position);
    isDragging = false;
    dragIsLeftButton = !event.mods.isRightButtonDown();
}

void HighwayComponent::mouseUp(const juce::MouseEvent& event)
{
    if (!writeMode || frameData.isPlaying)
        return;

    if (isDragging)
    {
        // Drag complete — resolve end position
        auto endHit = performHitTest(event.position);
        if (dragIsLeftButton && dragStartResult.valid && endHit.valid && onDragComplete)
        {
            onDragComplete(dragStartResult.timeFromCursor, dragStartResult.laneIndex,
                           endHit.timeFromCursor, endHit.laneIndex);
        }
        isDragging = false;
        repaint();
        return;
    }

    // Click (not drag) — check note existence and fire appropriate callback
    if (!dragStartResult.valid || dragStartResult.laneIndex < 0)
    {
        // Clicked outside highway
        if (dragIsLeftButton && onLeftClick)
            onLeftClick(0.0, -1, false);
        return;
    }

    double noteTime; int noteLane;
    bool noteExists = findNoteAtPosition(dragStartResult.normalizedPosition, dragStartResult.laneIndex, noteTime, noteLane);
    double time = noteExists ? noteTime : dragStartResult.timeFromCursor;
    int lane = noteExists ? noteLane : dragStartResult.laneIndex;

    if (dragIsLeftButton)
    {
        if (onLeftClick) onLeftClick(time, lane, noteExists);
    }
    else
    {
        if (onRightClick) onRightClick(time, lane, noteExists);
    }
}

void HighwayComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (!writeMode || frameData.isPlaying)
        return;

    if (!isDragging)
    {
        float dist = event.position.getDistanceFrom(mouseDownScreenPos);
        if (dist >= dragDistanceThreshold)
            isDragging = true;
        else
            return;
    }

    // Update hover for live lane tracking during drag
    auto hit = performHitTest(event.position);
    hoverResult = hit;
    hoverValid = hit.valid;
    repaint();
}

void HighwayComponent::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (!writeMode || frameData.isPlaying)
        return;

    auto hit = performHitTest(event.position);
    if (!hit.valid || hit.laneIndex < 0)
        return;

    double noteTime; int noteLane;
    bool noteExists = findNoteAtPosition(hit.normalizedPosition, hit.laneIndex, noteTime, noteLane);
    double time = noteExists ? noteTime : hit.timeFromCursor;
    int lane = noteExists ? noteLane : hit.laneIndex;

    if (onDoubleClick) onDoubleClick(time, lane, noteExists);
}

bool HighwayComponent::keyPressed(const juce::KeyPress& key)
{
    if (!writeMode || frameData.isPlaying)
        return false;

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (onKeyAction) onKeyAction(KA_DELETE);
        return true;
    }

    if (key == juce::KeyPress::upKey)
    {
        if (onKeyAction) onKeyAction(KA_MOVE_UP);
        return true;
    }

    if (key == juce::KeyPress::downKey)
    {
        if (onKeyAction) onKeyAction(KA_MOVE_DOWN);
        return true;
    }

    if (key == juce::KeyPress::leftKey)
    {
        if (onKeyAction) onKeyAction(KA_LANE_LEFT);
        return true;
    }

    if (key == juce::KeyPress::rightKey)
    {
        if (onKeyAction) onKeyAction(KA_LANE_RIGHT);
        return true;
    }

    return false;
}
