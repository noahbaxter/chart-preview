/*
    ==============================================================================

        GridlineRenderer.cpp
        Author:  Noah Baxter

        Gridline rendering extracted from SceneRenderer.

    ==============================================================================
*/

#include "GridlineRenderer.h"
#include "../Utils/RenderTypeConfig.h"
#include "../Utils/Frame.h"
#include "../Utils/FrameRenderer.h"
#include "../../UI/Theme.h"

using namespace PositionConstants;
using namespace Render;

GridlineRenderer::GridlineRenderer(juce::ValueTree& state, AssetManager& assetManager)
    : state(state), assetManager(assetManager)
{
}

void GridlineRenderer::populate(DrawCallMap& drawCallMap, const TimeBasedGridlineMap& gridlines,
                                double windowStartTime, double windowEndTime,
                                uint width, uint height,
                                float posEnd,
                                float gridlinePosOffset, float gridZOffset,
                                float farFadeEnd, float farFadeLen, float farFadeCurve)
{
    this->width = width;
    this->height = height;
    this->posEnd = posEnd;

    bool isDrums = isDrumLike(activePart);
    const auto* config = getRenderTypeConfig(getRenderType(activePart));
    const auto& fbCoords = *config->fretboardCoords;
    auto perspParams = config->getPerspectiveParams();

    // Strike-reference geometry — same for every gridline at this resolution.
    auto strikeEdge = getColumnEdge(0.0f, fbCoords, PositionConstants::GRIDLINE_WIDTH_SCALE);
    float strikeWidth = strikeEdge.rightX - strikeEdge.leftX;
    float strikeHeight = (perspParams.barNoteHeightRatio > 0.0f)
                       ? (strikeWidth / perspParams.barNoteHeightRatio)
                       : strikeWidth;

    double windowTimeSpan = windowEndTime - windowStartTime;

    // ------------------------------------------------------------------
    // Pre-pass: build the label set with priority-based density filtering.
    // MEASUREs always pass; BEATs pass only if far enough from any kept
    // MEASURE; HALF_BEATs pass only if far enough from any kept MEASURE-or-
    // BEAT. Threshold compares NORMALIZED HIGHWAY POSITION (linear in
    // time / scroll speed) — NOT screen Y — so perspective compression at
    // the back of the highway doesn't aggressively cull labels. The rule
    // is "labels are at least N measures of music apart" regardless of
    // where each gridline currently sits on the perspective curve.
    //
    // Labels are emitted in a separate post-pass below; the main loop
    // below renders gridlines + protrusions only.
    // ------------------------------------------------------------------
    struct LabelCandidate {
        size_t gridlineIdx;
        int    priority;            // 0=MEASURE, 1=BEAT, 2=HALF_BEAT
        float  widthRatio;
        float  normalizedPosition;
        juce::String text;
    };
    std::vector<LabelCandidate> labelCandidates;
    std::map<size_t, juce::String> labelToRender;  // gridlineIdx -> label text

    // Format helper: BEAT/HALF_BEAT/STEP labels are "M.<beatPart>", where
    // beatPart is the integer beat for an exact beat ("3"), or beat+fraction
    // for sub-beat divisions ("3.5", "3.25", "3.125", "3.333" for triplets).
    // Trims trailing zeros. MEASURE labels use just "M".
    auto formatBeatPart = [](double bim) -> juce::String {
        int beatInt = (int)std::floor(bim + 1e-6);
        double frac = bim - (double)beatInt;
        if (std::abs(frac) < 0.001) return juce::String(beatInt);
        juce::String fracStr = juce::String(frac, 3);   // "0.500" / "0.250" / "0.333"
        if (fracStr.startsWith("0")) fracStr = fracStr.substring(1);
        while (fracStr.endsWith("0") && fracStr.length() > 1)
            fracStr = fracStr.dropLastCharacters(1);
        return juce::String(beatInt) + fracStr;
    };

    if (writeMode && !PositionMath::bemaniMode)
    {
        for (size_t i = 0; i < gridlines.size(); ++i)
        {
            const auto& gl = gridlines[i];
            if (gl.measureNumber < 0) continue;

            int prio;
            juce::String text;
            if (gl.type == Gridline::MEASURE)
            {
                prio = 0;
                text = juce::String(gl.measureNumber + 1);
            }
            else if (gl.beatInMeasure > 0.0)
            {
                // Priority order: BEAT > HALF_BEAT > STEP. Tighter divisions
                // sit lower in priority so the density filter prefers
                // structural beats first.
                if      (gl.type == Gridline::BEAT)      prio = 1;
                else if (gl.type == Gridline::HALF_BEAT) prio = 2;
                else if (gl.type == Gridline::STEP)      prio = 3;
                else continue;
                text = juce::String(gl.measureNumber + 1) + "." + formatBeatPart(gl.beatInMeasure);
            }
            else
            {
                continue;
            }

            float normPos = (float)((gl.time - windowStartTime) / windowTimeSpan) + gridlinePosOffset;
            if (normPos < HIGHWAY_POS_START || normPos > farFadeEnd) continue;

            auto edge = getColumnEdge(normPos, fbCoords, PositionConstants::GRIDLINE_WIDTH_SCALE);
            float curWidth = edge.rightX - edge.leftX;
            float wr = (strikeWidth > 0.0f) ? (curWidth / strikeWidth) : 1.0f;

            labelCandidates.push_back({i, prio, wr, normPos, text});
        }

        // Sort by priority asc (MEASURE=0 first), then by index (time order)
        // for stable, predictable filtering within a priority class.
        std::sort(labelCandidates.begin(), labelCandidates.end(),
                  [](const LabelCandidate& a, const LabelCandidate& b) {
                      if (a.priority != b.priority) return a.priority < b.priority;
                      return a.gridlineIdx < b.gridlineIdx;
                  });

        // Greedy: accept a candidate if its normalizedPosition is at least
        // `threshold` away from every already-accepted normalizedPosition.
        // Threshold derives from the single visibility knob — see comment
        // on WRITE_LABEL_TARGET_COUNT in DrawingConstants.h.
        const float threshold = 1.0f / std::max(1.0f, WRITE_LABEL_TARGET_COUNT);
        std::vector<float> keptNorm;
        keptNorm.reserve(labelCandidates.size());
        bool anySubMeasureKept = false;
        for (const auto& c : labelCandidates)
        {
            bool farEnough = true;
            for (float kn : keptNorm)
            {
                if (std::abs(c.normalizedPosition - kn) < threshold) { farEnough = false; break; }
            }
            if (farEnough)
            {
                keptNorm.push_back(c.normalizedPosition);
                labelToRender[c.gridlineIdx] = c.text;
                if (c.priority > 0) anySubMeasureKept = true;
            }
        }

        // Precision parity: if any sub-measure label (BEAT/HALF_BEAT/STEP)
        // survived, MEASURE labels gain a ".1" suffix so the right-side
        // column reads consistently — "474.1" next to "474.2" / "474.3"
        // beats more naturally than a bare "474" that looks like it lost
        // its sub-beat number.
        if (anySubMeasureKept)
        {
            for (const auto& c : labelCandidates)
            {
                if (c.priority != 0) continue;
                auto it = labelToRender.find(c.gridlineIdx);
                if (it != labelToRender.end()) it->second = c.text + ".1";
            }
        }
    }

    for (const auto& gridline : gridlines)
    {
        double gridlineTime = gridline.time;
        Gridline gridlineType = gridline.type;

        float normalizedPosition = (float)((gridlineTime - windowStartTime) / windowTimeSpan) + gridlinePosOffset;

        if (normalizedPosition < HIGHWAY_POS_START || normalizedPosition > farFadeEnd)
            continue;

        // No renderer-side type filter in write mode — the generator already
        // decides which MEASURE/BEAT/HALF_BEAT lines emit (HALF_BEAT and BEAT
        // are filtered to step-grid alignment in the generator). All four
        // types render here at their per-type write-mode opacity.

        juce::Image* markerImage = assetManager.getGridlineImage(gridlineType);
        if (markerImage == nullptr)
            continue;

        float baseOpacity = 1.0f;
        if (writeMode)
        {
            switch (gridlineType) {
                case Gridline::MEASURE:    baseOpacity = WRITE_MEASURE_OPACITY;   break;
                case Gridline::BEAT:       baseOpacity = WRITE_BEAT_OPACITY;      break;
                case Gridline::HALF_BEAT:  baseOpacity = WRITE_HALF_BEAT_OPACITY; break;
                case Gridline::STEP:       baseOpacity = WRITE_STEP_OPACITY;      break;
            }
        }
        else
        {
            switch (gridlineType) {
                case Gridline::MEASURE:    baseOpacity = MEASURE_OPACITY;   break;
                case Gridline::BEAT:       baseOpacity = BEAT_OPACITY;      break;
                case Gridline::HALF_BEAT:  baseOpacity = HALF_BEAT_OPACITY; break;
                case Gridline::STEP:       baseOpacity = HALF_BEAT_OPACITY; break; // unreachable: not emitted when writeMode off
            }
        }
        float fadeOpacity = PositionMath::bemaniMode
                          ? 1.0f
                          : calculateFarFade(normalizedPosition, farFadeEnd, farFadeLen, farFadeCurve);

        // In write mode, MEASURE / BEAT are structural anchors — fading them
        // with distance lets dense STEP grids visually drown them at typical
        // viewing positions. Keep anchors at full per-type opacity; only the
        // STEP subgrid fades with depth.
        if (writeMode && (gridlineType == Gridline::MEASURE || gridlineType == Gridline::BEAT))
            fadeOpacity = 1.0f;

        float opacity = baseOpacity * fadeOpacity;

        if (PositionMath::bemaniMode)
        {
            // Bemani gridline = horizontal gradient line, no marker image. Push
            // the draw call directly; doesn't fit the Frame model.
            float bemaniOpacity = std::min(1.0f, opacity * bemaniConfig.gridlineBoost);
            uint w = width;
            uint h = height;
            float pe = posEnd;
            Part part = activePart;
            float pos = normalizedPosition;
            drawCallMap[(int)DrawOrder::GRID][0].push_back([w, h, pe, part, pos, bemaniOpacity](juce::Graphics& g) {
                bool drums = isDrumLike(part);
                auto edge = PositionMath::getFretboardEdge(drums, pos, w, h,
                                PositionConstants::HIGHWAY_POS_START, pe);
                float lineH = std::max(1.0f, (float)w * 0.003f);
                float lineY = edge.centerY - lineH * 0.5f + bemaniConfig.gridlineZ;

                juce::ColourGradient grad(
                    juce::Colours::white.withAlpha(bemaniOpacity), (edge.leftX + edge.rightX) * 0.5f, lineY,
                    juce::Colours::white.withAlpha(bemaniOpacity * 0.3f), edge.leftX, lineY, false);
                grad.addColour(0.0, juce::Colours::white.withAlpha(bemaniOpacity * 0.3f));
                grad.addColour(0.5, juce::Colours::white.withAlpha(bemaniOpacity));
                grad.addColour(1.0, juce::Colours::white.withAlpha(bemaniOpacity * 0.3f));
                g.setGradientFill(grad);
                g.fillRect(edge.leftX, lineY, edge.rightX - edge.leftX, lineH);
            });
            continue;
        }

        // Perspective path: single-sprite Frame at the gridline's depth.
        auto edge = getColumnEdge(normalizedPosition, fbCoords, PositionConstants::GRIDLINE_WIDTH_SCALE);
        float curWidth = edge.rightX - edge.leftX;
        float widthRatio = (strikeWidth > 0.0f) ? (curWidth / strikeWidth) : 1.0f;

        // In write mode, swap MEASURE/BEAT to a brighter "write" version of
        // the marker image. The boosted images have their alpha amplified at
        // load time in AssetManager, so we get full opacity through the same
        // perspective sprite path (no parallel rendering code).
        bool writeAnchor = writeMode && (gridlineType == Gridline::MEASURE || gridlineType == Gridline::BEAT);
        if (writeMode && gridlineType == Gridline::MEASURE)
            markerImage = assetManager.getMarkerMeasureWriteImage();
        else if (writeMode && gridlineType == Gridline::BEAT)
            markerImage = assetManager.getMarkerBeatWriteImage();
        if (markerImage == nullptr) markerImage = assetManager.getGridlineImage(gridlineType);

        juce::Point<float> anchor((edge.leftX + edge.rightX) * 0.5f, edge.centerY);
        juce::Point<float> frameScale(widthRatio, widthRatio);

        Frame frame;

        FrameSprite s;
        s.image     = markerImage;
        s.offsetX   = 0.0f;
        s.offsetY   = gridZOffset;
        s.width     = strikeWidth;
        s.height    = strikeHeight;
        // All gridlines render at GRID — above the highway texture, BEHIND
        // the track sidebars / lane lines / strikeline so the rails clip
        // the marker's 12% overhang. View-mode parity for the on-highway
        // line; MEASURE/BEAT distinguish themselves via the side
        // protrusions below, not via on-highway prominence treatments.
        s.drawOrder = (int)DrawOrder::GRID;
        s.drawColumn = 0;
        s.opacity   = opacity;
        frame.sprites.push_back(s);

        drawFrame(frame, anchor, frameScale, drawCallMap);

        // Side-of-highway protrusions: flat white tabs sticking outward from
        // the highway edges at every write-mode MEASURE/BEAT. Drawn as direct
        // fillRect — they're NOT on the highway plane (they protrude past the
        // edges), so the perspective sprite path doesn't apply. Same GRID
        // draw order as the on-highway line: track frame contains the tabs
        // visually where they meet the rim. STEP gets no protrusion — the
        // step grid is the placement grid, kept visually quiet.
        if (writeAnchor)
        {
            bool drums = isDrumLike(activePart);
            float pos = normalizedPosition;
            float zoff = gridZOffset;
            uint w = width, h = height;
            float pe = posEnd;
            float wr = widthRatio;
            float sw = strikeWidth;
            float op = opacity;
            float lengthFrac = (gridlineType == Gridline::MEASURE)
                             ? WRITE_PROTRUSION_MEASURE_LENGTH_FRAC
                             : WRITE_PROTRUSION_BEAT_LENGTH_FRAC;
            float thickFrac  = (gridlineType == Gridline::MEASURE)
                             ? WRITE_PROTRUSION_MEASURE_THICKNESS_FRAC
                             : WRITE_PROTRUSION_BEAT_THICKNESS_FRAC;
            drawCallMap[(int)DrawOrder::GRID][0].push_back([drums, pos, zoff, w, h, pe, wr, sw, op, lengthFrac, thickFrac](juce::Graphics& g) {
                auto fbEdge = PositionMath::getFretboardEdge(drums, pos, w, h,
                                PositionConstants::HIGHWAY_POS_START, pe);
                float thickness = std::max(1.5f, sw * thickFrac * wr);
                float length    = std::max(4.0f, sw * lengthFrac * wr);
                float zNudge = sw * WRITE_PROTRUSION_Z_NUDGE_FRAC * wr;
                float lineY = fbEdge.centerY + zoff * wr + zNudge - thickness * 0.5f;
                g.setColour(juce::Colours::white.withAlpha(op));
                g.fillRect(fbEdge.leftX  - length, lineY, length, thickness);
                g.fillRect(fbEdge.rightX,           lineY, length, thickness);
            });
        }

    }

    // ------------------------------------------------------------------
    // Post-pass: emit kept labels. Each label uses MEASURE-protrusion
    // anchoring so the right-side label column aligns horizontally
    // regardless of the gridline's type. Labels render at full alpha
    // (legibility > matching the gridline's faded opacity).
    // ------------------------------------------------------------------
    for (const auto& c : labelCandidates)
    {
        auto it = labelToRender.find(c.gridlineIdx);
        if (it == labelToRender.end()) continue;

        bool drums = isDrumLike(activePart);
        float pos  = c.normalizedPosition;
        float zoff = gridZOffset;
        uint  w    = width, h_ = height;
        float pe   = posEnd;
        float wr   = c.widthRatio;
        float sw   = strikeWidth;
        juce::String labelText = it->second;

        drawCallMap[(int)DrawOrder::GRID][0].push_back([drums, pos, zoff, w, h_, pe, wr, sw, labelText](juce::Graphics& g) {
            float fontPx = sw * WRITE_MEASURE_LABEL_FONT_FRAC * wr;
            if (fontPx < WRITE_MEASURE_LABEL_MIN_FONT_PX) return;
            auto fbEdge = PositionMath::getFretboardEdge(drums, pos, w, h_,
                            PositionConstants::HIGHWAY_POS_START, pe);
            // Anchor past the MEASURE protrusion tip — keeps the label
            // column horizontally aligned across MEASURE/BEAT/HALF_BEAT.
            float protLen = sw * WRITE_PROTRUSION_MEASURE_LENGTH_FRAC * wr;
            float gap     = sw * WRITE_MEASURE_LABEL_GAP_FRAC * wr;
            float textX = fbEdge.rightX + protLen + gap;
            float textY = fbEdge.centerY + zoff * wr;
            float textBoxW = fontPx * 6.0f;  // generous: fits "999.4.5" comfortably
            float textBoxH = fontPx * 1.2f;
            g.setColour(juce::Colours::white);
            g.setFont(Theme::getUIFont(fontPx));
            g.drawText(labelText,
                       juce::Rectangle<float>(textX, textY - textBoxH * 0.5f, textBoxW, textBoxH),
                       juce::Justification::centredLeft, false);
        });
    }
}
