/*
  ==============================================================================

    SceneRenderer.cpp
    Created: 15 Jun 2024 3:57:32pm
    Author:  Noah Baxter

  ==============================================================================
*/

#include "SceneRenderer.h"
#include "../Utils/DrawingConstants.h"
#include "../Utils/PositionConstants.h"
#include "../../UI/Theme.h"

using namespace PositionConstants;

SceneRenderer::SceneRenderer(juce::ValueTree &state, AssetManager &assetManager)
	: state(state),
	  assetManager(assetManager),
	  noteRenderer(state, assetManager),
	  sustainRenderer(state, assetManager),
	  gridlineRenderer(state, assetManager),
	  animationRenderer(state, assetManager)
{
    std::copy_n(PositionConstants::OVERLAY_DEFAULTS, PositionConstants::NUM_OVERLAY_TYPES, overlayAdjusts);
}

SceneRenderer::~SceneRenderer()
{
}

void SceneRenderer::paint(juce::Graphics &g, int viewportWidth, int viewportHeight, const TimeBasedTrackWindow& trackWindow, const TimeBasedSustainWindow& sustainWindow, const TimeBasedGridlineMap& gridlines, const TimeBasedFlipRegions& flipRegions, const TimeBasedEventMarkers& eventMarkers, double windowStartTime, double windowEndTime, bool isPlaying)
{
    ScopedPhaseMeasure totalMeasure(lastPhaseTiming.total_us, collectPhaseTiming);

    // Use the full viewport size, not clip bounds — partial repaints must not
    // change the perspective math or the scene will render incorrectly.
    width = viewportWidth;
    height = viewportHeight;

    bool isDrums = isDrumLike(activePart);

    // Propagate activePart to sub-renderers
    noteRenderer.activePart = activePart;
    sustainRenderer.activePart = activePart;
    gridlineRenderer.activePart = activePart;
    animationRenderer.activePart = activePart;

    // Update sustain states for active animations (force-trigger if playing into active sustain)
    animationRenderer.updateSustainStates(sustainWindow, isPlaying);

    // Clear animations when paused to indicate gem is not being played
    if (!isPlaying)
    {
        animationRenderer.reset();
    }

    // Repopulate drawCallMap
    for (auto& row : drawCallMap)
        for (auto& bucket : row)
            bucket.clear();

    // Resolution scale: all pixel-based Z offsets are tuned at REFERENCE_HEIGHT
    float resScale = (float)height / PositionConstants::REFERENCE_HEIGHT;

    const auto& offsets = isDrums ? drumOffsets : guitarOffsets;

    noteRenderer.noteCurvatureGuitar = noteCurvatureGuitar;
    noteRenderer.noteCurvatureDrums = noteCurvatureDrums;
    noteRenderer.gemScale = isDrums ? drumGemScale : guitarGemScale;
    noteRenderer.barScale = isDrums ? drumBarScale : guitarBarScale;
    float strikePosGem = offsets.strikePosGem;
    float strikePosBar = offsets.strikePosBar;

    noteRenderer.showGems = showGems;
    noteRenderer.showBars = showBars;
    noteRenderer.gemZOffset = offsets.gemZ * resScale;
    noteRenderer.cymZOffset = offsets.cymZ * resScale;
    noteRenderer.barZOffset = offsets.barZ * resScale;
    noteRenderer.strikePosGem = strikePosGem;
    noteRenderer.strikePosBar = strikePosBar;
    noteRenderer.gemTypeScales = gemTypeScales;
    noteRenderer.overlayAdjusts = overlayAdjusts;       // pointer to scene-side array
    noteRenderer.guitarColAdjust = guitarColAdjust;
    noteRenderer.drumColAdjust = drumColAdjust;
    noteRenderer.resScale = resScale;                    // applied to ColumnAdjust::z reads
    noteRenderer.laneCoordsGuitar = guitarLaneCoordsLocal;
    noteRenderer.laneCoordsDrums = drumLaneCoordsLocal;

    {
        ScopedPhaseMeasure m(lastPhaseTiming.notes_us, collectPhaseTiming);
        if (showGems || showBars)
            noteRenderer.populate(drawCallMap, trackWindow, windowStartTime, windowEndTime,
                                  width, height, highwayPosEnd,
                                  farFadeEnd, farFadeLen, farFadeCurve);

        if (ghostCursor.visible)
        {
            noteRenderer.renderGhost(drawCallMap, ghostCursor.lane, ghostCursor.position,
                                     ghostCursor.image, ghostCursor.opacity);

            if (ghostCursor.positionLabel.isNotEmpty())
            {
                bool drums = isDrumLike(activePart);
                float pos = ghostCursor.position;
                float pe = highwayPosEnd;
                uint w = width, h = height;
                juce::String label = ghostCursor.positionLabel;
                auto strikeEdge = PositionMath::getFretboardEdge(drums, 0.0f, w, h, HIGHWAY_POS_START, pe);
                float sw = strikeEdge.rightX - strikeEdge.leftX;

                drawCallMap[(int)DrawOrder::OVERLAY][0].push_back([drums, pos, w, h, pe, sw, label](juce::Graphics& g) {
                    auto fbEdge = PositionMath::getFretboardEdge(drums, pos, w, h,
                                      PositionConstants::HIGHWAY_POS_START, pe);
                    float wr = (sw > 0.0f) ? ((fbEdge.rightX - fbEdge.leftX) / sw) : 1.0f;
                    float fontPx = sw * WRITE_MEASURE_LABEL_FONT_FRAC * wr;
                    if (fontPx < WRITE_MEASURE_LABEL_MIN_FONT_PX) return;

                    float protLen = sw * WRITE_PROTRUSION_MEASURE_LENGTH_FRAC * wr;
                    float gap     = sw * WRITE_MEASURE_LABEL_GAP_FRAC * wr;
                    float textX = fbEdge.leftX - protLen - gap;
                    float textY = fbEdge.centerY;
                    float textBoxW = fontPx * 6.0f;
                    float textBoxH = fontPx * 1.2f;
                    g.setColour(juce::Colours::white.withAlpha(0.9f));
                    g.setFont(Theme::getUIFont(fontPx));
                    g.drawText(label,
                               juce::Rectangle<float>(textX - textBoxW, textY - textBoxH * 0.5f, textBoxW, textBoxH),
                               juce::Justification::centredRight, false);
                });
            }
        }

        for (const auto& sel : selectionHighlights)
            noteRenderer.renderSelectionTint(drawCallMap, sel.lane, sel.position);
        selectionHighlights.clear();
    }

    {
        ScopedPhaseMeasure m(lastPhaseTiming.sustains_us, collectPhaseTiming);
        sustainRenderer.laneShape = laneShape;
        sustainRenderer.populate(drawCallMap, sustainWindow, windowStartTime, windowEndTime,
                                 width, height, showLanes, showSustains,
                                 highwayPosEnd,
                                 farFadeEnd, farFadeLen, farFadeCurve,
                                 guitarLaneCoordsLocal, drumLaneCoordsLocal);
    }

    {
        ScopedPhaseMeasure m(lastPhaseTiming.gridlines_us, collectPhaseTiming);
        if (showGridlines)
            gridlineRenderer.populate(drawCallMap, gridlines, windowStartTime, windowEndTime,
                                      width, height, highwayPosEnd,
                                      gridlinePosOffset,
                                      offsets.gridZ * resScale,
                                      farFadeEnd, farFadeLen, farFadeCurve);
    }

    // Text event markers (disco flip start/end)
    if (!flipRegions.empty())
    {
        textEventRenderer.activePart = activePart;
        textEventRenderer.populate(drawCallMap, flipRegions, windowStartTime, windowEndTime,
                                   width, height, highwayPosEnd,
                                   farFadeEnd, farFadeLen, farFadeCurve);
    }

    // TODO: enable event marker rendering (tempo/timesig changes, sections, lyrics, etc.)
    // textEventRenderer.activePart = activePart;
    // textEventRenderer.populateEventMarkers(drawCallMap, eventMarkers, windowStartTime, windowEndTime,
    //                                        width, height, highwayPosEnd,
    //                                        farFadeEnd, farFadeLen, farFadeCurve);

    // Detect and add animations to drawCallMap (if enabled)
    {
        ScopedPhaseMeasure m(lastPhaseTiming.animation_us, collectPhaseTiming);
        bool hitIndicatorsEnabled = state.getProperty("hitIndicators");
        if (hitIndicatorsEnabled)
        {
            double windowTimeSpan = windowEndTime - windowStartTime;
            double strikeTimeOffset = strikePosGem * windowTimeSpan;
            if (isPlaying) { animationRenderer.detectAndTriggerAnimations(trackWindow, strikeTimeOffset); }

            animationRenderer.laneCoordsGuitar = guitarLaneCoordsLocal;
            animationRenderer.laneCoordsDrums = drumLaneCoordsLocal;
            animationRenderer.hitGemZOffset = offsets.hitGemZ * resScale;
            animationRenderer.hitBarZOffset = offsets.hitBarZ * resScale;
            animationRenderer.noteCurvature = isDrums ? noteCurvatureDrums : noteCurvatureGuitar;
            animationRenderer.hitGemScale = hitGemScale;
            animationRenderer.hitBarScale = hitBarScale;
            animationRenderer.hitTypeConfig = hitTypeConfig;
            animationRenderer.drumColAdjust = drumColAdjust;
            animationRenderer.resScale = resScale;

            animationRenderer.renderToDrawCallMap(drawCallMap, width, height, highwayPosEnd,
                                                strikePosGem);
        }
    }

    // Inject registered image overlays into drawCallMap
    for (auto& kv : overlays)
    {
        auto order = kv.first;
        auto* img = kv.second;
        if (!img || !img->isValid()) continue;

        // Skip track/strikeline overlays when toggled off
        if (!showTrack && (order == DrawOrder::TRACK_SIDEBARS || order == DrawOrder::TRACK_CONNECTORS))
            continue;
        if (!showLaneSeparators && order == DrawOrder::TRACK_LANE_LINES)
            continue;
        if (!showStrikeline && order == DrawOrder::TRACK_STRIKELINE)
            continue;

        int vw = width, vh = height;
        int ov = overlayYOffset;
        int totalH = vh + ov;
        drawCallMap[static_cast<int>(order)][0].push_back([img, vw, totalH, ov](juce::Graphics& g) {
            g.setOpacity(1.0f);
            int imgW = img->getWidth(), imgH = img->getHeight();
            if (imgW == vw && imgH == totalH)
                g.drawImageAt(*img, 0, -ov);
            else
                g.drawImage(*img, 0, -ov, vw, totalH, 0, 0, imgW, imgH);
        });
    }

    // Inject custom draw calls (e.g. bemani sidebar rails)
    for (auto& entry : customDrawCalls)
    {
        if (entry.second)
        {
            auto& drawFn = entry.second;
            drawCallMap[static_cast<int>(entry.first)][0].push_back([&drawFn](juce::Graphics& g) { drawFn(g); });
        }
    }

    // Draw layer by layer, then column by column within each layer
    {
        ScopedPhaseMeasure m(lastPhaseTiming.execute_us, collectPhaseTiming);
        if (collectPhaseTiming)
            std::fill(std::begin(lastPhaseTiming.layer_us), std::end(lastPhaseTiming.layer_us), 0.0);

        for (int d = 0; d < DRAW_ORDER_COUNT; d++)
        {
            ScopedPhaseMeasure lm(lastPhaseTiming.layer_us[d], collectPhaseTiming);
            for (int c = 0; c < MAX_DRAW_COLUMNS; c++)
            {
                auto& bucket = drawCallMap[d][c];
                // Draw each layer from back to front
                for (auto it = bucket.rbegin(); it != bucket.rend(); ++it)
                {
                    (*it)(g);
                }
            }
        }
    }

    // Advance animation frames after rendering (time-based)
    {
        double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        double delta = (lastFrameTimeSeconds > 0.0) ? (now - lastFrameTimeSeconds) : (1.0 / 60.0);
        delta = juce::jlimit(0.001, 0.1, delta);  // Clamp to avoid huge jumps
        lastFrameTimeSeconds = now;

        bool hitIndicatorsEnabled = state.getProperty("hitIndicators");
        if (hitIndicatorsEnabled)
        {
            animationRenderer.advanceFrames(delta);
        }
    }
}
