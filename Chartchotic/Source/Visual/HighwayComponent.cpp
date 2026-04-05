/*
    ==============================================================================

        HighwayComponent.cpp
        Author: Noah Baxter

    ==============================================================================
*/

#include "HighwayComponent.h"
#include "TrackImageCache.h"
#include "../UI/ControlConstants.h"
#include "../UI/Theme.h"
#include "Painters/LanePainter.h"

namespace {
    // Sustain drag preview
    constexpr float SUSTAIN_PREVIEW_OPACITY     = 0.45f;
    constexpr float SUSTAIN_PREVIEW_OPACITY_DIM = 0.15f;
    constexpr float DRAG_GEM_OPACITY            = 0.75f;

    // Note hit detection
    constexpr double NOTE_HIT_TOLERANCE = 0.05;

    // Guide line rendering
    constexpr float GUIDE_LINE_GLOW_ALPHA = 0.12f;
    constexpr float GUIDE_LINE_CORE_ALPHA = 0.5f;
    constexpr float GUIDE_LINE_GLOW_WIDTH = 5.0f;
    constexpr float GUIDE_LINE_CORE_WIDTH = 1.5f;
    constexpr float GUIDE_LINE_Y_NUDGE    = 0.35f;
}

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

    paintSustainDragPreview(g);

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

    paintDragGemHead(g);
    paintHoverCursor(g);
    paintSelectionHighlight(g);
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

// =============================================================================
// Coordinate conversion helpers
// =============================================================================

double HighwayComponent::windowTimeSpan() const
{
    return frameData.windowEndTime - frameData.windowStartTime;
}

float HighwayComponent::timeToNormalized(double time) const
{
    double span = windowTimeSpan();
    if (span <= 0.0) return 0.0f;
    return (float)((time - frameData.windowStartTime) / span);
}

double HighwayComponent::normalizedToTime(float pos) const
{
    return (double)pos * windowTimeSpan() + frameData.windowStartTime;
}

HighwayComponent::LaneVisuals HighwayComponent::resolveLaneVisuals(int lane) const
{
    using namespace PositionConstants;
    bool isDrums = isDrumLike(activePart);
    uint gemCol;
    NormalizedCoordinates laneCoords;
    if (isDrums) {
        gemCol = (lane == 0) ? 0 : (uint)lane;
        laneCoords = drumBezierLaneCoords[gemCol];
    } else {
        gemCol = (uint)lane;
        laneCoords = guitarBezierLaneCoords[gemCol];
    }
    return { gemCol, laneCoords };
}

// =============================================================================
// Snap and note lookup
// =============================================================================

float HighwayComponent::snapToNearestGridline(float normalizedPos) const
{
    if (windowTimeSpan() <= 0.0 || frameData.gridlines.empty())
        return normalizedPos;

    float bestPos = normalizedPos;
    float bestDist = std::numeric_limits<float>::max();

    for (const auto& gl : frameData.gridlines)
    {
        float glPos = timeToNormalized(gl.time);
        float dist = std::abs(glPos - normalizedPos);
        if (dist < bestDist)
        {
            bestDist = dist;
            bestPos = glPos;
        }
    }
    return bestPos;
}

float HighwayComponent::snapToNearestGridlineOrNote(float normalizedPos, int laneIndex) const
{
    float best = writeHints.snapEnabled ? snapToNearestGridline(normalizedPos) : normalizedPos;
    float bestDist = std::abs(best - normalizedPos);

    if (windowTimeSpan() > 0.0 && laneIndex >= 0 && laneIndex < (int)LANE_COUNT)
    {
        for (const auto& [time, frame] : frameData.trackWindow)
        {
            if (frame[(size_t)laneIndex].gem == Gem::NONE) continue;
            float notePos = timeToNormalized(time);
            float dist = std::abs(notePos - normalizedPos);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = notePos;
            }
        }
    }
    return best;
}

float HighwayComponent::findNextNotePosition(float afterNormalizedPos, int laneIndex) const
{
    if (laneIndex < 0 || laneIndex >= (int)LANE_COUNT)
        return -1.0f;
    if (windowTimeSpan() <= 0.0)
        return -1.0f;

    double targetTime = normalizedToTime(afterNormalizedPos);
    auto it = frameData.trackWindow.upper_bound(targetTime);
    while (it != frameData.trackWindow.end())
    {
        if (it->second[(size_t)laneIndex].gem != Gem::NONE)
            return timeToNormalized(it->first);
        ++it;
    }
    return -1.0f;
}

std::vector<float> HighwayComponent::findNotePositionsInRange(float fromPos, float toPos, int laneIndex) const
{
    std::vector<float> positions;
    if (laneIndex < 0 || laneIndex >= (int)LANE_COUNT)
        return positions;
    if (windowTimeSpan() <= 0.0)
        return positions;

    double fromTime = normalizedToTime(fromPos);
    double toTime = normalizedToTime(toPos);

    for (const auto& [time, frame] : frameData.trackWindow)
    {
        if (time < fromTime - 0.01) continue;
        if (time > toTime + 0.01) break;
        if (frame[(size_t)laneIndex].gem != Gem::NONE)
            positions.push_back(timeToNormalized(time));
    }
    return positions;
}

// =============================================================================
// Extracted paint helpers (write mode)
// =============================================================================

void HighwayComponent::paintSustainDragPreview(juce::Graphics& g)
{
    if (writeMode && drag.active && drag.isLeftButton && drag.startResult.valid && hoverValid && !frameData.isPlaying)
    {
        int dragLane = hoverResult.valid ? hoverResult.laneIndex : drag.startResult.laneIndex;
        float startPos = snapToNearestGridlineOrNote(drag.startResult.normalizedPosition, dragLane);
        float hoverPos = snapToNearestGridlineOrNote(hoverResult.normalizedPosition, dragLane);

        if (hoverPos > startPos && dragLane >= 0)
        {
            using namespace PositionConstants;
            auto laneVis = resolveLaneVisuals(dragLane);
            uint gemCol = laneVis.gemCol;
            auto laneCoords = laneVis.laneCoords;
            bool isBar = isBarNote(gemCol, activePart);
            float endOffset = isBar ? BAR_SUSTAIN_END_OFFSET : SUSTAIN_END_OFFSET;
            float sustW = isBar ? SUSTAIN_OPEN_WIDTH : SUSTAIN_WIDTH;
            float laneScale = isBar ? BAR_SIZE : GEM_SIZE;
            auto colour = assetManager.getLaneColour(gemCol, activePart, false);

            struct SustainSegment { float segStart; float segEnd; };
            std::vector<SustainSegment> segments;

            auto notePositions = findNotePositionsInRange(startPos - 0.01f, hoverPos, dragLane);
            if (notePositions.size() > 1)
            {
                for (size_t n = 0; n < notePositions.size(); n++)
                {
                    float segStart = notePositions[n];
                    float segEnd = (n + 1 < notePositions.size())
                        ? notePositions[n + 1] + endOffset
                        : hoverPos + endOffset;
                    // Only cap at next-note-beyond-cascade for the last segment;
                    // middle segments already end at notePositions[n+1] which IS
                    // the next note. Calling findNextNotePosition for every segment
                    // hits a float precision bug: the float→double round-trip can
                    // cause upper_bound to return the SAME note, crushing the segment.
                    if (n + 1 >= notePositions.size())
                    {
                        float nextAfter = findNextNotePosition(segStart, dragLane);
                        if (nextAfter > 0.0f && segEnd > nextAfter + endOffset)
                            segEnd = nextAfter + endOffset;
                    }
                    segEnd = std::max(segStart + 0.01f, segEnd);
                    segments.push_back({ segStart, segEnd });
                }
            }
            else
            {
                float adjustedEnd = std::max(startPos + 0.01f, hoverPos + endOffset);
                float nextNotePos = findNextNotePosition(startPos, dragLane);
                if (nextNotePos > 0.0f && adjustedEnd > nextNotePos + endOffset)
                    adjustedEnd = std::max(startPos + 0.01f, nextNotePos + endOffset);
                segments.push_back({ startPos, adjustedEnd });
            }

            float minSustNorm = writeHints.minSustainNormalized;
            float posEnd = sceneRenderer.highwayPosEnd;
            float fadeEnd = sceneRenderer.farFadeEnd;
            float fadeLen = sceneRenderer.farFadeLen;
            float fadeCurve = sceneRenderer.farFadeCurve;
            int rw = renderWidth, rh = renderHeight;
            Part part = activePart;

            sceneRenderer.setCustomDrawCall(DrawOrder::SUSTAIN,
                [segments, gemCol, part, sustW, colour, laneCoords, laneScale,
                 posEnd, rw, rh, fadeEnd, fadeLen, fadeCurve, minSustNorm](juce::Graphics& g2)
            {
                for (const auto& seg : segments)
                {
                    float segLen = seg.segEnd - seg.segStart;
                    bool belowMin = (minSustNorm > 0.0f && segLen < minSustNorm);
                    float opacity = belowMin ? SUSTAIN_PREVIEW_OPACITY_DIM : SUSTAIN_PREVIEW_OPACITY;

                    LanePainter::Params lp {
                        gemCol, part,
                        seg.segStart, seg.segEnd,
                        opacity, sustW, colour, false,
                        (uint)rw, (uint)rh, posEnd,
                        laneCoords, laneScale, -1, {},
                        fadeEnd, fadeLen, fadeCurve
                    };
                    LanePainter::paint(g2, lp);
                }
            });
        }
        else
        {
            sceneRenderer.setCustomDrawCall(DrawOrder::SUSTAIN, {});
        }
    }
    else
    {
        sceneRenderer.setCustomDrawCall(DrawOrder::SUSTAIN, {});
    }
}

void HighwayComponent::paintDragGemHead(juce::Graphics& g)
{
    if (!(writeMode && drag.active && drag.isLeftButton && drag.startResult.valid && hoverValid && !frameData.isPlaying))
        return;

    int dragLane = hoverResult.valid ? hoverResult.laneIndex : drag.startResult.laneIndex;
    if (dragLane < 0) return;

    float startPos = snapToNearestGridlineOrNote(drag.startResult.normalizedPosition, dragLane);

    auto [gemCol, laneCoords] = resolveLaneVisuals(dragLane);
    GemWrapper defaultGem(Gem::NOTE, false);
    juce::Image* glyphImage = isGuitarLike(activePart)
        ? assetManager.getGuitarGlyphImage(defaultGem, gemCol, false)
        : assetManager.getDrumGlyphImage(defaultGem, gemCol, false);
    if (glyphImage == nullptr) return;

    float aspect = (float)glyphImage->getWidth() / (float)glyphImage->getHeight();
    auto gp = sceneRenderer.noteRenderCtx.buildGemParams(startPos, gemCol, aspect);

    g.saveState();
    g.addTransform(getRenderTransform());
    if (topOverflow > 0)
        g.addTransform(juce::AffineTransform::translation(0.0f, (float)topOverflow));

    NotePainter::paintGem(g, gp, glyphImage, nullptr, DRAG_GEM_OPACITY,
                          sceneRenderer.noteCurvatureGuitar, sceneRenderer.noteCurvatureDrums,
                          sceneRenderer.guitarLaneCoordsLocal, sceneRenderer.drumLaneCoordsLocal);
    g.restoreState();
}

void HighwayComponent::paintHoverCursor(juce::Graphics& g)
{
    if (!(writeMode && hoverValid && !drag.active && !frameData.isPlaying))
        return;

    float rawPos = hoverResult.normalizedPosition;
    float snappedPos = (writeHints.drawMode && writeHints.snapEnabled)
        ? snapToNearestGridlineOrNote(rawPos, hoverResult.laneIndex)
        : rawPos;
    auto ov = computeNoteOverlay(snappedPos, hoverResult.laneIndex);
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
        g.strokePath(ghostPath, juce::PathStrokeType(GUIDE_LINE_CORE_WIDTH));
    }

    // Guide line across fretboard
    using namespace PositionConstants;
    bool isDrums = isDrumLike(activePart);
    auto lineOv = writeHints.freeCursor
        ? computeNoteOverlay(rawPos, hoverResult.laneIndex)
        : ov;
    auto renderXform = getRenderTransform();

    auto fbEdge = PositionMath::getFretboardEdge(
        isDrums, lineOv.position, (uint)renderWidth, (uint)renderHeight,
        HIGHWAY_POS_START, HIGHWAY_POS_END);
    auto fbScreenL = juce::Point<float>(fbEdge.leftX, 0).transformedBy(renderXform);
    auto fbScreenR = juce::Point<float>(fbEdge.rightX, 0).transformedBy(renderXform);
    float fbScreenLeft  = fbScreenL.x;
    float fbScreenRight = fbScreenR.x;
    float fbScreenMid   = (fbScreenLeft + fbScreenRight) * 0.5f;

    float lineNudge = lineOv.screenH * GUIDE_LINE_Y_NUDGE;
    float lineY = lineOv.screenCenterY + lineNudge;

    if (std::abs(lineOv.curvature) > 0.001f)
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
            yOff[i] = fbWidthPx * lineOv.curvature * (1.0f - d * d) * lineOv.sy;
        }

        juce::Path linePath;
        linePath.startNewSubPath(fbScreenLeft, lineY + yOff[0]);
        linePath.quadraticTo(fbScreenMid, lineY + yOff[1],
                             fbScreenRight, lineY + yOff[2]);

        g.setColour(juce::Colours::white.withAlpha(GUIDE_LINE_GLOW_ALPHA));
        g.strokePath(linePath, juce::PathStrokeType(GUIDE_LINE_GLOW_WIDTH));
        g.setColour(juce::Colours::white.withAlpha(GUIDE_LINE_CORE_ALPHA));
        g.strokePath(linePath, juce::PathStrokeType(GUIDE_LINE_CORE_WIDTH));
    }
    else
    {
        g.setColour(juce::Colours::white.withAlpha(GUIDE_LINE_GLOW_ALPHA));
        g.drawLine(fbScreenLeft, lineY, fbScreenRight, lineY, GUIDE_LINE_GLOW_WIDTH);
        g.setColour(juce::Colours::white.withAlpha(GUIDE_LINE_CORE_ALPHA));
        g.drawLine(fbScreenLeft, lineY, fbScreenRight, lineY, GUIDE_LINE_CORE_WIDTH);
    }
}

void HighwayComponent::paintSelectionHighlight(juce::Graphics& g)
{
    if (!(writeMode && hasSelection && selectedLane >= 0 && !frameData.isPlaying))
        return;

    float selPos = (windowTimeSpan() > 0.0) ? timeToNormalized(selectedTime) : 0.0f;

    double foundTime; int foundLane;
    if (selPos >= PositionConstants::HIGHWAY_POS_START && selPos <= sceneRenderer.farFadeEnd
        && findNoteAtPosition(selPos, selectedLane, foundTime, foundLane))
    {
        float foundPos = (windowTimeSpan() > 0.0) ? timeToNormalized(foundTime) : selPos;

        auto ov = computeNoteOverlay(foundPos, selectedLane);
        auto selPath = buildCurvedNotePath(ov, 2.0f);

        g.setColour(juce::Colour(0x4000ddff));
        g.fillPath(selPath);
        g.setColour(juce::Colour(0xff00ddff));
        g.strokePath(selPath, juce::PathStrokeType(2.0f));
    }
}

// =============================================================================
// Write mode state
// =============================================================================

void HighwayComponent::setWriteMode(bool on)
{
    writeMode = on;
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

juce::AffineTransform HighwayComponent::getRenderTransform() const
{
    int w = renderWidth;
    int totalH = renderHeight + topOverflow;
    if (w <= 0 || totalH <= 0)
        return {};

    if (stretchToFill && !PositionMath::bemaniMode)
    {
        float sx = (float)getWidth() / (float)w;
        float sy = (float)getHeight() / (float)totalH;
        return juce::AffineTransform::scale(sx, sy);
    }
    else
    {
        float scale = std::min((float)getWidth() / (float)w,
                               (float)getHeight() / (float)totalH);
        float offsetX = ((float)getWidth() - (float)w * scale) / 2.0f;
        float offsetY = (float)getHeight() - (float)totalH * scale;
        return juce::AffineTransform(scale, 0.0f, offsetX, 0.0f, scale, offsetY);
    }
}

juce::Point<float> HighwayComponent::screenToRenderCoords(juce::Point<float> screen) const
{
    return screen.transformedBy(getRenderTransform().inverted());
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
    if (windowTimeSpan() <= 0.0)
        return false;

    double targetTime = normalizedToTime(normalizedPosition);

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

    if (std::abs((double)timeToNormalized(bestTime) - (double)normalizedPosition) > NOTE_HIT_TOLERANCE)
        return false;

    outTime = bestTime;
    outLane = laneIndex;
    return true;
}

bool HighwayComponent::findSustainAtPosition(float normalizedPosition, int laneIndex,
                                              double& outStartTime) const
{
    if (laneIndex < 0 || laneIndex >= (int)LANE_COUNT)
        return false;
    if (windowTimeSpan() <= 0.0)
        return false;

    double targetTime = normalizedToTime(normalizedPosition);

    for (const auto& sustain : frameData.sustainWindow)
    {
        if (sustain.sustainType != SustainType::SUSTAIN) continue;
        if ((int)sustain.gemColumn != laneIndex) continue;

        // Check if click time falls within sustain body (excluding note head area)
        if (targetTime > sustain.startTime && targetTime < sustain.endTime)
        {
            outStartTime = sustain.startTime;
            return true;
        }
    }
    return false;
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
    // Don't clear hover state during an active drag — focus changes
    // (alt-tab, etc.) fire mouseExit but the drag is still valid
    if (drag.active) return;

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
    drag.mouseDownScreenPos = event.position;
    drag.startResult = performHitTest(event.position);
    drag.active = false;
    drag.isLeftButton = !event.mods.isRightButtonDown();
}

void HighwayComponent::mouseUp(const juce::MouseEvent& event)
{
    if (!writeMode || frameData.isPlaying)
        return;

    if (drag.active)
    {
        // Drag complete — resolve end position
        auto endHit = performHitTest(event.position);
        if (drag.isLeftButton && drag.startResult.valid && endHit.valid && onDragComplete)
        {
            onDragComplete(drag.startResult.timeFromCursor, drag.startResult.laneIndex,
                           endHit.timeFromCursor, endHit.laneIndex);
        }
        drag.active = false;
        repaint();
        return;
    }

    // Click (not drag) — check note existence and fire appropriate callback
    if (!drag.startResult.valid || drag.startResult.laneIndex < 0)
    {
        // Clicked outside highway
        if (drag.isLeftButton && onLeftClick)
            onLeftClick(0.0, -1, false);
        return;
    }

    double noteTime; int noteLane;
    bool noteExists = findNoteAtPosition(drag.startResult.normalizedPosition, drag.startResult.laneIndex, noteTime, noteLane);
    double time = noteExists ? noteTime : drag.startResult.timeFromCursor;
    int lane = noteExists ? noteLane : drag.startResult.laneIndex;

    if (drag.isLeftButton)
    {
        if (onLeftClick) onLeftClick(time, lane, noteExists);
    }
    else
    {
        // Right-click: check sustain body if no note head was hit
        if (!noteExists)
        {
            double sustainStart;
            if (findSustainAtPosition(drag.startResult.normalizedPosition,
                                      drag.startResult.laneIndex, sustainStart))
            {
                if (onSustainRightClick)
                    onSustainRightClick(sustainStart, drag.startResult.laneIndex);
                return;
            }
        }
        if (onRightClick) onRightClick(time, lane, noteExists);
    }
}

void HighwayComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (!writeMode || frameData.isPlaying)
        return;

    if (!drag.active)
    {
        float dist = event.position.getDistanceFrom(drag.mouseDownScreenPos);
        if (dist >= DragState::distanceThreshold)
            drag.active = true;
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
