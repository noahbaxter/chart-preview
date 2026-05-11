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
#include "../Midi/Utils/MidiConstants.h"

HighwayComponent::HighwayComponent(juce::ValueTree& state, AssetManager& assetManager)
    : state(state),
      assetManager(assetManager),
      sceneRenderer(state, assetManager),
      trackRenderer(state)
{
    // Sync activePart from state
    setActivePart(getPartFromState(state));
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

    // Ghost cursor: set before paint so it renders through the note pipeline.
    sceneRenderer.ghostCursor.visible = false;
    sceneRenderer.ghostCursor.positionLabel = {};
    if (overlayStateGetter)
    {
        const auto& ov = overlayStateGetter();
        if (ov.ghostVisible && projectQNToSeconds)
        {
            double windowSpan = frameData.windowEndTime - frameData.windowStartTime;
            if (std::abs(windowSpan) > 1e-9)
            {
                double sec = projectQNToSeconds(ov.ghostQN);
                float pos = (float)((sec - frameData.windowStartTime) / windowSpan);
                sceneRenderer.ghostCursor.position = pos;
                sceneRenderer.ghostCursor.positionLabel = formatPositionQN
                    ? formatPositionQN(ov.ghostQN) : juce::String();

                sceneRenderer.ghostCursor.stampGhosts.clear();
                if (!ov.stampGhosts.empty())
                {
                    for (const auto& sg : ov.stampGhosts)
                    {
                        double sgSec = projectQNToSeconds(ov.ghostQN + sg.qnOffset);
                        float sgPos = (float)((sgSec - frameData.windowStartTime) / windowSpan);
                        sceneRenderer.ghostCursor.stampGhosts.push_back({ sg.lane, sgPos });
                    }
                }
                else if (ov.ghostLane >= 0)
                {
                    sceneRenderer.ghostCursor.visible = true;
                    sceneRenderer.ghostCursor.lane = ov.ghostLane;
                    sceneRenderer.ghostCursor.gem = ov.ghostGem;
                    sceneRenderer.ghostCursor.image = sceneRenderer.useColoredGhostCursor
                        ? nullptr
                        : assetManager.getGhostCursorImage(isDrumLike(activePart), ov.ghostLane);
                }
            }
        }
    }

    // Populate selection state for edit mode.
    // Selection tint is applied inline during gem rendering (via NoteRenderer::selectedGems).
    // Move preview ghosts are rendered separately at destination positions.
    sceneRenderer.getSelectedGems().clear();
    sceneRenderer.getEraseTargets().clear();
    sceneRenderer.getTintedSustains().clear();
    sceneRenderer.movePreviewGhosts.clear();
    if (overlayStateGetter && projectQNToSeconds)
    {
        const auto& ov = overlayStateGetter();
        double windowSpan = frameData.windowEndTime - frameData.windowStartTime;
        if (std::abs(windowSpan) > 1e-9)
        {
            if (ov.moveDragVisible)
            {
                bool autoHopo = !isDrumLike(activePart)
                    && state.hasProperty("autoHopo") && (bool)state["autoHopo"];
                PPQ hopoThreshold = PPQ(0.0);
                if (autoHopo)
                {
                    int ti = (int)state.getProperty("hopoThresh", HOPO_THRESHOLD_DEFAULT);
                    switch (ti)
                    {
                        case 0:  hopoThreshold = MIDI_HOPO_SIXTEENTH;  break;
                        case 2:  hopoThreshold = MIDI_HOPO_EIGHTH;     break;
                        default: hopoThreshold = MIDI_HOPO_CLASSIC_170; break;
                    }
                    hopoThreshold += MIDI_HOPO_THRESHOLD_BUFFER;
                }

                for (const auto& pn : ov.movePreviewNotes)
                {
                    double sec = projectQNToSeconds(pn.startQN);
                    float pos = (float)((sec - frameData.windowStartTime) / windowSpan);

                    Gem gem = Gem::NOTE;
                    if (autoHopo && secondsToProjectQN)
                    {
                        double qn = secondsToProjectQN(sec);
                        auto it = frameData.trackWindow.lower_bound(sec);
                        if (it != frameData.trackWindow.begin())
                        {
                            --it;
                            double prevQN = secondsToProjectQN(it->first);
                            PPQ dist = PPQ(qn - prevQN);

                            int prevLaneCount = 0, prevLane = -1;
                            for (int l = 0; l < (int)it->second.size(); ++l)
                                if (it->second[l].gem != Gem::NONE) { ++prevLaneCount; prevLane = l; }

                            bool prevChord = (prevLaneCount >= 2);
                            if (dist > PPQ(0.0) && dist <= hopoThreshold
                                && !prevChord && pn.lane != prevLane)
                                gem = Gem::HOPO_GHOST;
                        }
                    }

                    sceneRenderer.movePreviewGhosts.push_back({ pn.lane, pos, gem });
                }
            }
            else if (!ov.selectedNotes.empty())
            {
                for (const auto& sn : ov.selectedNotes)
                {
                    double sec = projectQNToSeconds(sn.startQN);
                    if (!sn.sustainOnly)
                        sceneRenderer.getSelectedGems().push_back({ sn.lane, sec });
                    sceneRenderer.getTintedSustains().push_back(
                        { sn.lane, sec, sec + 9999.0, AuthoringColours::selectTint });
                }
            }

            // Marquee preview tint: select = blue, erase = red
            if (ov.marqueeVisible)
            {
                const auto& mr = ov.marqueeRect;
                auto& targets = ov.marqueeErase
                    ? sceneRenderer.getEraseTargets()
                    : sceneRenderer.getSelectedGems();
                double secLo = projectQNToSeconds(mr.qnLo);
                double secHi = projectQNToSeconds(mr.qnHi);
                for (const auto& [noteTime, frame] : frameData.trackWindow)
                {
                    double qn = secondsToProjectQN(noteTime);
                    if (qn < mr.qnLo - 0.001 || qn > mr.qnHi + 0.001) continue;
                    for (int lane = mr.laneLo; lane <= mr.laneHi; ++lane)
                    {
                        if (ov.barMode && lane != 0) continue;
                        if (lane >= 0 && lane < (int)frame.size()
                            && frame[lane].gem != Gem::NONE)
                            targets.push_back({ lane, noteTime });
                    }
                }
                auto sustainCol = ov.marqueeErase
                    ? AuthoringColours::eraseTint : AuthoringColours::selectTint;
                for (const auto& s : frameData.sustainWindow)
                {
                    int lane = (int)s.gemColumn;
                    if (ov.barMode && lane != 0) continue;
                    if (lane < mr.laneLo || lane > mr.laneHi) continue;
                    if (s.startTime > secHi + 0.002 || s.endTime < secLo - 0.002) continue;
                    sceneRenderer.getTintedSustains().push_back(
                        { lane, secLo, secHi, sustainCol });
                }
            }

            // Tint the specific clicked note/sustain (may be outside the rect bounds)
            if (ov.marqueeErase && ov.eraseClickedNoteQN >= 0.0 && ov.eraseClickedLane >= 0)
            {
                double sec = projectQNToSeconds(ov.eraseClickedNoteQN);
                sceneRenderer.getEraseTargets().push_back({ ov.eraseClickedLane, sec });
                sceneRenderer.getTintedSustains().push_back(
                    { ov.eraseClickedLane, sec, sec, AuthoringColours::eraseTint });
            }
            if (ov.marqueeErase && ov.eraseClickedSustainQN >= 0.0 && ov.eraseClickedSustainLane >= 0)
            {
                double sec = projectQNToSeconds(ov.eraseClickedSustainQN);
                sceneRenderer.getTintedSustains().push_back(
                    { ov.eraseClickedSustainLane, sec, sec, AuthoringColours::eraseTint });
            }
        }
    }

    // Inject sustain drag preview into a local copy of the sustain window.
    auto sustainWindow = frameData.sustainWindow;
    if (overlayStateGetter && projectQNToSeconds)
    {
        const auto& ov = overlayStateGetter();
        if (ov.drawPreviewVisible)
        {
            for (const auto& pn : ov.drawPreviewNotes)
            {
                sustainWindow.push_back({
                    projectQNToSeconds(pn.startQN),
                    projectQNToSeconds(pn.endQN),
                    static_cast<uint>(pn.lane),
                    SustainType::SUSTAIN,
                    GemWrapper()
                });
            }
        }

        // Move drag: hide original sustains, show preview sustains at destination
        if (ov.moveDragVisible)
        {
            constexpr double kTol = 0.002;
            for (const auto& sn : ov.selectedNotes)
            {
                double sec = projectQNToSeconds(sn.startQN);
                sustainWindow.erase(
                    std::remove_if(sustainWindow.begin(), sustainWindow.end(),
                        [&](const auto& s) {
                            return s.gemColumn == (uint)sn.lane
                                && std::abs(s.startTime - sec) < kTol;
                        }),
                    sustainWindow.end());
            }
            for (const auto& pn : ov.movePreviewNotes)
            {
                if (pn.endQN - pn.startQN < double(MIDI_MIN_SUSTAIN_LENGTH)) continue;
                sustainWindow.push_back({
                    projectQNToSeconds(pn.startQN),
                    projectQNToSeconds(pn.endQN),
                    static_cast<uint>(pn.lane),
                    SustainType::SUSTAIN,
                    GemWrapper()
                });
            }
        }
    }

    // Apply optimistic patches — instant visual feedback before MIDI pipeline catches up.
    auto trackWindow = frameData.trackWindow;
    if (projectQNToSeconds)
    {
        constexpr double kMatchTol = 0.002;
        double windowSpan = frameData.windowEndTime - frameData.windowStartTime;

        // Hide originals during an active move drag
        if (overlayStateGetter)
        {
            const auto& ov = overlayStateGetter();
            if (ov.moveDragVisible)
            {
                for (const auto& sn : ov.selectedNotes)
                {
                    double sec = projectQNToSeconds(sn.startQN);
                    for (auto& [noteTime, frame] : trackWindow)
                        if (std::abs(noteTime - sec) < kMatchTol
                            && sn.lane >= 0 && sn.lane < (int)frame.size())
                            frame[sn.lane].gem = Gem::NONE;
                }
            }
        }

        if (patchBuffer)
        {
            for (const auto& patch : patchBuffer->getRemoves())
            {
                double sec = projectQNToSeconds(patch.startQN);
                for (auto& [noteTime, frame] : trackWindow)
                    if (std::abs(noteTime - sec) < kMatchTol
                        && patch.lane >= 0 && patch.lane < (int)frame.size())
                        frame[patch.lane].gem = Gem::NONE;

                sustainWindow.erase(
                    std::remove_if(sustainWindow.begin(), sustainWindow.end(),
                        [&](const auto& s) {
                            return (int)s.gemColumn == patch.lane
                                && std::abs(s.startTime - sec) < kMatchTol;
                        }),
                    sustainWindow.end());
            }

            for (const auto& patch : patchBuffer->getAdds())
            {
                double sec = projectQNToSeconds(patch.startQN);
                bool alreadyExists = false;
                for (const auto& [noteTime, frame] : trackWindow)
                {
                    if (std::abs(noteTime - sec) < kMatchTol
                        && patch.lane >= 0 && patch.lane < (int)frame.size()
                        && frame[patch.lane].gem != Gem::NONE)
                    { alreadyExists = true; break; }
                }
                if (!alreadyExists && std::abs(windowSpan) > 1e-9)
                {
                    float pos = (float)((sec - frameData.windowStartTime) / windowSpan);
                    sceneRenderer.movePreviewGhosts.push_back({ patch.lane, pos });
                }
            }
        }
    }

    bool barModeActive = overlayStateGetter && overlayStateGetter().barMode;
    sceneRenderer.setBarModeDim(barModeActive ? 0.3f : 1.0f);

    sceneRenderer.paint(g, w, h,
                        trackWindow, sustainWindow, frameData.gridlines,
                        frameData.flipRegions, frameData.eventMarkers,
                        frameData.windowStartTime, frameData.windowEndTime, frameData.isPlaying);

#ifdef DEBUG
    if (showClickZones)
    {
        bool dbgBarMode = overlayStateGetter && overlayStateGetter().barMode;
        Part dbgPart = isDrumLike(activePart) ? Part::DRUMS : Part::GUITAR;
        g.setColour(juce::Colours::lime.withAlpha(0.35f));
        for (const auto& hb : sceneRenderer.getNoteHitBoxes())
            if (isBarNote((uint)hb.lane, dbgPart) == dbgBarMode)
                g.drawEllipse(hb.rect, 1.5f);

        if (debugLastHitY >= 0.0f)
        {
            int ly = (int)debugLastHitY;
            g.setColour(juce::Colours::red.withAlpha(0.8f));
            g.drawHorizontalLine(ly, 0.0f, (float)w);
            juce::String label = (formatPositionQN ? formatPositionQN(debugLastResolvedQN)
                                                   : juce::String(debugLastResolvedQN, 2))
                + "  lane=" + juce::String(debugLastLane);
            g.setFont(20.0f);
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.fillRect(2, ly - 26, 380, 24);
            g.setColour(juce::Colours::red);
            g.drawText(label, 6, ly - 26, 372, 24, juce::Justification::left);
        }
    }
#endif

    // Edit-mode overlays (marquee, selection highlight)
    if (overlayStateGetter && projectQNToSeconds)
    {
        const auto& ov = overlayStateGetter();
        double windowSpan = frameData.windowEndTime - frameData.windowStartTime;
        bool isDrums = isDrumLike(activePart);
        float posEnd = sceneRenderer.farFadeEnd;

        auto qnToPos = [&](double qn) -> float {
            double sec = projectQNToSeconds(qn);
            return (windowSpan > 1e-9)
                ? (float)((sec - frameData.windowStartTime) / windowSpan)
                : 0.0f;
        };

        auto laneEdges = [&](int lane, float pos) -> PositionConstants::LaneCorners {
            int laneCount = isDrums ? 5 : 6;
            int clampedLane = juce::jlimit(0, laneCount - 1, lane);
            const auto& coords = isDrums
                ? PositionConstants::drumBezierLaneCoords[clampedLane]
                : PositionConstants::guitarBezierLaneCoords[clampedLane];
            return PositionMath::getColumnPosition(isDrums, pos, (uint)w, (uint)h,
                PositionConstants::HIGHWAY_POS_START, posEnd,
                coords, 1.0f, PositionConstants::FRETBOARD_SCALE);
        };

        // Marquee rectangle (blue = select, red = erase)
        if (ov.marqueeVisible)
        {
            const auto& mr = ov.marqueeRect;
            float posTop = qnToPos(mr.qnHi);
            float posBot = qnToPos(mr.qnLo);

            auto topLeft  = laneEdges(mr.laneLo, posTop);
            auto topRight = laneEdges(mr.laneHi, posTop);
            auto botLeft  = laneEdges(mr.laneLo, posBot);
            auto botRight = laneEdges(mr.laneHi, posBot);

            juce::Path marquee;
            marquee.startNewSubPath(topLeft.leftX, topLeft.centerY);
            marquee.lineTo(topRight.rightX, topRight.centerY);
            marquee.lineTo(botRight.rightX, botRight.centerY);
            marquee.lineTo(botLeft.leftX, botLeft.centerY);
            marquee.closeSubPath();

            auto col = ov.marqueeErase ? juce::Colour(255, 80, 80)
                                       : juce::Colour(100, 180, 255);
            g.setColour(col.withAlpha(0.15f));
            g.fillPath(marquee);
            g.setColour(col.withAlpha(0.5f));
            g.strokePath(marquee, juce::PathStrokeType(1.5f));
        }

    }

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

// =============================================================================
// Mouse dispatch (M1.3) — turns JUCE mouse events into AuthoringPoint/Context
// payloads and forwards them to write-mode callbacks. Callbacks are no-op stubs
// in M1; this is purely the wire-up.
// =============================================================================

juce::Point<float> HighwayComponent::screenToRenderCoords(juce::Point<float> screen) const
{
    int w        = renderWidth;
    int h        = renderHeight;
    int overflow = topOverflow;
    int totalH   = h + overflow;

    if (w <= 0 || totalH <= 0)
        return screen;

    if (stretchToFill && !PositionMath::bemaniMode)
    {
        float sx = (float)getWidth()  / (float)w;
        float sy = (float)getHeight() / (float)totalH;
        return { screen.x / sx, screen.y / sy };
    }
    else
    {
        float scale = std::min((float)getWidth()  / (float)w,
                               (float)getHeight() / (float)totalH);
        float scaledW = (float)w * scale;
        float scaledH = (float)totalH * scale;
        float offsetX = ((float)getWidth() - scaledW) * 0.5f;
        float offsetY = (float)getHeight() - scaledH;
        return { (screen.x - offsetX) / scale, (screen.y - offsetY) / scale };
    }
}

void HighwayComponent::buildAuthoringPayload(const juce::MouseEvent& e,
                                             AuthoringPoint& outPoint,
                                             AuthoringContext& outContext) const
{
    auto local = e.getPosition().toFloat();

    // The hit-test math operates in render space (renderWidth x renderHeight
    // virtual canvas). Component-local pixels must be inverse-mapped through
    // the same paint() transform the ghost render uses, otherwise the click
    // resolves to a different render-space position than the ghost was drawn
    // at and ghost/click drift apart (especially under stretchToFill or any
    // letterboxing).
    auto renderPt = screenToRenderCoords(local);
    float hitY = renderPt.y - (float)topOverflow;

#ifdef DEBUG
    debugLastHitY = hitY;
    debugLastLane = -1;
    debugLastResolvedQN = 0.0;
#endif

    bool isDrums = isDrumLike(activePart);
    auto hit = hitTestMapper.hitTest(renderPt.x, hitY,
                                     (uint)juce::jmax(0, renderWidth),
                                     (uint)juce::jmax(0, renderHeight),
                                     frameData.windowStartTime, frameData.windowEndTime,
                                     isDrums, sceneRenderer.farFadeEnd);

    outPoint.screenPos = local;
    if (hit.valid && hit.laneIndex >= 0)
    {
        outPoint.onHighway = true;
        outPoint.laneIndex = hit.laneIndex;
    }
    else
    {
        outPoint.onHighway = false;
        outPoint.laneIndex = -1;
    }

    bool barMode = overlayStateGetter && overlayStateGetter().barMode;
    if (barMode && outPoint.onHighway)
        outPoint.laneIndex = 0;

    // In bar mode, correct for barZ offset so ghost/placement center on the
    // click point instead of the top of the bar sprite.
    if (barMode && secondsToProjectQN)
    {
        float barZ = (isDrums ? PositionConstants::DRUM_OFFSETS : PositionConstants::GUITAR_OFFSETS).barZ;
        float resScale = (float)juce::jmax(1, renderHeight) / PositionConstants::REFERENCE_HEIGHT;
        auto barHit = hitTestMapper.hitTest(renderPt.x, hitY - barZ * resScale,
                                            (uint)juce::jmax(0, renderWidth),
                                            (uint)juce::jmax(0, renderHeight),
                                            frameData.windowStartTime, frameData.windowEndTime,
                                            isDrums, sceneRenderer.farFadeEnd);
        outPoint.rawProjectQN = secondsToProjectQN(barHit.timeFromCursor);
    }
    else
    {
        outPoint.rawProjectQN = secondsToProjectQN ? secondsToProjectQN(hit.timeFromCursor) : 0.0;
    }

#ifdef DEBUG
    debugLastResolvedQN = outPoint.rawProjectQN;
    debugLastLane = hit.laneIndex;
#endif

    outPoint.overExistingNote = false;
    outPoint.hitSustainBody   = false;
    outPoint.hitNoteStartQN   = 0.0;
    outPoint.hitNotePitch     = -1;

    if (outPoint.onHighway && secondsToProjectQN)
    {
        // Geometric hit test against rendered note rects.
        // Bar mode only matches bars; normal mode only matches gems.
        for (const auto& hb : sceneRenderer.getNoteHitBoxes())
        {
            if (isBarNote((uint)hb.lane, isDrums ? Part::DRUMS : Part::GUITAR) != barMode)
                continue;
            if (hb.rect.contains(renderPt.x, hitY))
            {
                outPoint.overExistingNote = true;
                outPoint.hitSustainBody   = false;
                outPoint.hitNoteStartQN   = secondsToProjectQN(hb.timeSec);
                outPoint.laneIndex        = hb.lane;
                break;
            }
        }

        // Sustain body check (no rendered rect — use time tolerance)
        if (!outPoint.overExistingNote)
        {
            constexpr float kHeadHitPixels = 32.0f;
            double windowSpan = frameData.windowEndTime - frameData.windowStartTime;
            double timeTol = (windowSpan > 0.0 && renderHeight > 0)
                ? kHeadHitPixels * windowSpan / (double)renderHeight
                : 0.05;
            double hitTime = hit.timeFromCursor;
            uint hitLane = (uint)outPoint.laneIndex;

            for (const auto& s : frameData.sustainWindow)
            {
                if (s.gemColumn != hitLane) continue;

                bool nearHead = std::abs(hitTime - s.startTime) < timeTol;
                bool inBody   = hitTime >= s.startTime && hitTime < s.endTime;

                if (nearHead || inBody)
                {
                    outPoint.overExistingNote = true;
                    outPoint.hitSustainBody   = inBody && !nearHead;
                    outPoint.hitNoteStartQN   = secondsToProjectQN(s.startTime);
                    break;
                }
            }
        }
    }

    outContext.mods = juce::ModifierKeys::getCurrentModifiers();
    outContext.leftButton  = e.mods.isLeftButtonDown();
    outContext.rightButton = e.mods.isRightButtonDown();
}

void HighwayComponent::mouseEnter(const juce::MouseEvent& e)
{
    if (!onPointerMove) return;
    AuthoringPoint p; AuthoringContext ctx;
    buildAuthoringPayload(e, p, ctx);
    onPointerMove(p, ctx);
}

void HighwayComponent::mouseExit(const juce::MouseEvent&)
{
    if (onPointerExit) onPointerExit();

    // Clear the hover ghost when the cursor leaves the highway — only repaint
    // if a ghost was actually being drawn.
    if (overlayStateGetter && lastGhost.visible)
    {
        lastGhost = LastGhostState{};
        repaint();
    }
}

void HighwayComponent::mouseMove(const juce::MouseEvent& e)
{
    if (!onPointerMove) return;
    AuthoringPoint p; AuthoringContext ctx;
    buildAuthoringPayload(e, p, ctx);
    onPointerMove(p, ctx);

    // Only repaint when the ghost actually changed. Vblank already handles
    // continuous repaints at 60fps; this avoids piling extra full-renders on
    // top during dense step grids (mouseMove fires at 100+ Hz).
    if (overlayStateGetter)
    {
        const auto& ov = overlayStateGetter();
        if (ov.ghostVisible != lastGhost.visible ||
            ov.ghostLane    != lastGhost.lane    ||
            ov.ghostQN      != lastGhost.qn)
        {
            lastGhost = { ov.ghostVisible, ov.ghostLane, ov.ghostQN };
            repaint();
        }
    }
}

void HighwayComponent::mouseDown(const juce::MouseEvent& e)
{
    if (!onPointerDown) return;
    AuthoringPoint p; AuthoringContext ctx;
    buildAuthoringPayload(e, p, ctx);
    onPointerDown(p, ctx);
}

void HighwayComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (!onPointerDrag) return;
    AuthoringPoint p; AuthoringContext ctx;
    buildAuthoringPayload(e, p, ctx);
    onPointerDrag(p, ctx);
}

void HighwayComponent::mouseUp(const juce::MouseEvent& e)
{
    if (!onPointerUp) return;
    AuthoringPoint p; AuthoringContext ctx;
    buildAuthoringPayload(e, p, ctx);
    onPointerUp(p, ctx);
}

void HighwayComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (!onPointerDoubleClick) return;
    AuthoringPoint p; AuthoringContext ctx;
    buildAuthoringPayload(e, p, ctx);
    onPointerDoubleClick(p, ctx);
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
