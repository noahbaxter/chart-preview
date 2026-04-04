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
#include "../Midi/Utils/InstrumentMapper.h"
#include "../UI/Theme.h"

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

    // Write mode hover cursor (disabled during playback)
    if (writeMode && hoverValid && !frameData.isPlaying)
    {
        auto ov = computeNoteOverlay(hoverResult.normalizedPosition, hoverResult.laneIndex);
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

HighwayComponent::NoteOverlay HighwayComponent::computeNoteOverlay(float position, int lane) const
{
    using namespace PositionConstants;
    NoteOverlay ov{};
    bool isDrums = isDrumLike(activePart);
    ov.isBar = (lane == 0);

    int w = renderWidth;
    int h = renderHeight;
    int overflow = topOverflow;
    int totalH = h + overflow;

    // Screen transform (mirrors paint())
    float sx, sy, ox, oy;
    if (stretchToFill && !PositionMath::bemaniMode)
    {
        sx = (float)getWidth() / (float)w;
        sy = (float)getHeight() / (float)totalH;
        ox = 0.0f; oy = 0.0f;
    }
    else
    {
        float scale = std::min((float)getWidth() / (float)w,
                               (float)getHeight() / (float)totalH);
        sx = scale; sy = scale;
        ox = ((float)getWidth() - (float)w * scale) / 2.0f;
        oy = (float)getHeight() - (float)totalH * scale;
    }
    ov.sy = sy;

    // Foreshortening
    float foreshorten = 1.0f;
    if (!PositionMath::bemaniMode)
    {
#ifdef DEBUG
        const auto& pp = PositionMath::perspParams(isDrums);
#else
        auto pp = getPerspectiveParams(isDrums);
#endif
        float depthPos = ov.isBar ? position + BAR_NOTE_POS_OFFSET : position;
        float depth = std::max(0.0f, depthPos) / pp.vanishingPointDepth;
        float scaleNear = 1.0f + (pp.highwayDepth / pp.playerDistance) * pp.perspectiveStrength;
        float psCur = scaleNear / (1.0f + depth * (scaleNear - 1.0f));
        float rawRatio = psCur / scaleNear;
        foreshorten = 1.0f - (1.0f - rawRatio) * NOTE_DEPTH_FORESHORTEN;
    }

    // Note rect in render space
    float rLeftX, rRightX, rCenterY, noteW, noteH;
    float renderPosition = ov.isBar ? position + BAR_NOTE_POS_OFFSET : position;
    if (ov.isBar)
    {
        auto fbEdge = PositionMath::getFretboardEdge(
            isDrums, renderPosition, (uint)renderWidth, (uint)renderHeight,
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
            isDrums, position, (uint)renderWidth, (uint)renderHeight,
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
    ov.curvature = isDrums ? sceneRenderer.noteCurvatureDrums
                           : sceneRenderer.noteCurvatureGuitar;
    ov.arcOffset = 0.0f;
    if (ov.curvature != 0.0f)
    {
        const auto& fbCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;
        float fbCenter = fbCoords.normX1 + fbCoords.normWidth1 * 0.5f;
        float fbHalfW = fbCoords.normWidth1 * 0.5f;
        float dist = 0.0f;
        if (!ov.isBar)
        {
            const auto* lc = isDrums ? drumBezierLaneCoords : guitarBezierLaneCoords;
            float colCenter = lc[lane].normX1 + lc[lane].normWidth1 * 0.5f;
            dist = (colCenter - fbCenter) / fbHalfW;
        }
        auto fbEdge = PositionMath::getFretboardEdge(
            isDrums, renderPosition, (uint)renderWidth, (uint)renderHeight,
            HIGHWAY_POS_START, HIGHWAY_POS_END);
        float fbWidthPx = (fbEdge.rightX - fbEdge.leftX) * FRETBOARD_SCALE;
        ov.arcOffset = fbWidthPx * ov.curvature * (1.0f - dist * dist);
    }

    ov.renderLeftX = rLeftX;
    ov.renderRightX = rRightX;
    ov.position = renderPosition;

    // Z offset: read from SceneRenderer (computed once per frame from InstrumentOffsets * resScale)
    float zOff = ov.isBar ? sceneRenderer.getBarZOffset() : sceneRenderer.getGemZOffset();

    // Transform to screen
    ov.screenLeftX  = rLeftX * sx + ox;
    ov.screenRightX = rRightX * sx + ox;
    ov.screenCenterY = (rCenterY + zOff + ov.arcOffset + (float)overflow) * sy + oy;
    ov.screenH = noteH * sy;

    return ov;
}

juce::Path HighwayComponent::buildCurvedNotePath(const NoteOverlay& ov, float expand) const
{
    using namespace PositionConstants;
    juce::Path p;
    float left  = ov.screenLeftX - expand;
    float right = ov.screenRightX + expand;
    float top   = ov.screenCenterY - ov.screenH * 0.5f - expand;
    float bot   = ov.screenCenterY + ov.screenH * 0.5f + expand;

    if (std::abs(ov.curvature) < 0.001f)
    {
        p.addRoundedRectangle(left, top, right - left, bot - top,
                              ov.isBar ? 2.0f : 3.0f);
    }
    else
    {
        bool isDrums = isDrumLike(activePart);
        auto fbEdge = PositionMath::getFretboardEdge(
            isDrums, ov.position, (uint)renderWidth, (uint)renderHeight,
            HIGHWAY_POS_START, HIGHWAY_POS_END);
        float fbWidthPx = (fbEdge.rightX - fbEdge.leftX) * FRETBOARD_SCALE;
        const auto& fbCoords = isDrums ? drumFretboardCoords : guitarFretboardCoords;
        float fbCenterNorm = fbCoords.normX1 + fbCoords.normWidth1 * 0.5f;
        float fbHalfWNorm = fbCoords.normWidth1 * 0.5f;

        float pts[3] = { ov.renderLeftX, (ov.renderLeftX + ov.renderRightX) * 0.5f, ov.renderRightX };
        float yOff[3];
        for (int i = 0; i < 3; i++)
        {
            float normX = pts[i] / (float)renderWidth;
            float d = (normX - fbCenterNorm) / fbHalfWNorm;
            float fullArc = fbWidthPx * ov.curvature * (1.0f - d * d);
            yOff[i] = (fullArc - ov.arcOffset) * ov.sy;
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

void HighwayComponent::setWriteMode(bool on, MidiWriter* writer, int trackIndex)
{
    writeMode = on;
    midiWriter = on ? writer : nullptr;
    writeTrackIndex = on ? trackIndex : -1;
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

    // Grab keyboard focus for DELETE/arrow keys
    grabKeyboardFocus();

    auto hit = performHitTest(event.position);
    if (!hit.valid || hit.laneIndex < 0)
    {
        if (onNoteClicked) onNoteClicked(0.0, -1, false);
        return;
    }

    // Check if there's an existing note at this position — controller handles PPQ resolution
    double noteTime; int noteLane;
    bool noteExists = findNoteAtPosition(hit.normalizedPosition, hit.laneIndex, noteTime, noteLane);

    if (onNoteClicked)
        onNoteClicked(noteExists ? noteTime : hit.timeFromCursor, noteExists ? noteLane : hit.laneIndex, noteExists);
}

void HighwayComponent::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (!writeMode || !midiWriter || writeTrackIndex < 0 || frameData.isPlaying)
        return;

    auto hit = performHitTest(event.position);

    if (!hit.valid || hit.laneIndex < 0)
        return;

    bool isDrums = isDrumLike(activePart);
    SkillLevel skill = (SkillLevel)(int)state.getProperty("skillLevel");
    auto pitches = isDrums
        ? InstrumentMapper::getDrumPitchesForSkill(skill)
        : InstrumentMapper::getGuitarPitchesForSkill(skill);

    if (hit.laneIndex >= (int)pitches.size())
        return;

    int pitch = (int)pitches[(size_t)hit.laneIndex];

    if (onNoteEditRequested)
        onNoteEditRequested(hit.timeFromCursor, pitch);
}

bool HighwayComponent::keyPressed(const juce::KeyPress& key)
{
    if (!writeMode || !hasSelection || frameData.isPlaying)
        return false;

    bool isDrums = isDrumLike(activePart);
    SkillLevel skill = (SkillLevel)(int)state.getProperty("skillLevel");
    auto pitches = isDrums
        ? InstrumentMapper::getDrumPitchesForSkill(skill)
        : InstrumentMapper::getGuitarPitchesForSkill(skill);

    if (selectedLane < 0 || selectedLane >= (int)pitches.size())
        return false;

    int pitch = (int)pitches[(size_t)selectedLane];

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (onNoteDeleteRequested)
            onNoteDeleteRequested(selectedTime, pitch);
        return true;
    }

    if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
    {
        if (onNoteMoveRequested)
        {
            int dir = key == juce::KeyPress::upKey ? 1 : -1;
            onNoteMoveRequested(selectedTime, pitch, dir);
            // Selection stays — WriteController updates selection.ppq in the callback
        }
        return true;
    }

    return false;
}
