#ifdef DEBUG

#include "DebugTuningPanel.h"
#include "../Visual/Renderers/SceneRenderer.h"
#include "../Visual/Managers/AssetManager.h"
#include <cstring>

// --- Shared helpers for data-driven slider sections ---

void DebugTuningPanel::initTunableSliders(ScrollableLabel* labels, const DebugTunable* tunables,
                                           int count, std::function<void()> onChange)
{
    for (int i = 0; i < count; i++)
    {
        const auto& t = tunables[i];
        setupScrollLabel(labels[i]);
        // Monospace so name + value columns line up across rows.
        static const auto monoFont = juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain);
        labels[i].setFont(monoFont);
        if (t.featured)
            labels[i].setColour(juce::Label::textColourId, juce::Colour(0xFF4FC3F7));
        auto fmtRow = [](const char* name, float v, int dec) {
            juce::String s;
            s << "  " << name;
            while (s.length() < 24) s << " ";
            s << juce::String(v, dec);
            return s;
        };
        labels[i].setText(fmtRow(t.name, *t.value, t.decimals), juce::dontSendNotification);
        auto apply = [this, i, &t = tunables[i], onChange, labels, fmtRow](float newVal) {
            *t.value = juce::jlimit(t.min, t.max, newVal);
            labels[i].setText(fmtRow(t.name, *t.value, t.decimals), juce::dontSendNotification);
            if (onChange) onChange();
        };
        labels[i].onScroll = [&t = tunables[i], apply](int delta) {
            apply(*t.value + delta * t.step);
        };
        labels[i].onSet = apply;
    }
}

void DebugTuningPanel::addTunableChildren(ScrollableLabel* labels, int count)
{
    for (int i = 0; i < count; i++)
        tuningButton.addPanelChild(&labels[i]);
}

void DebugTuningPanel::layoutTunableRows(ScrollableLabel* labels, int count, bool visible,
                                          int margin, int w, int rowHeight, int gap, int& y)
{
    for (int i = 0; i < count; i++)
    {
        labels[i].setVisible(visible);
        if (visible)
        {
            labels[i].setBounds(margin, y, w, rowHeight);
            y += rowHeight + gap;
        }
    }
}

void DebugTuningPanel::refreshTunableLabels(ScrollableLabel* labels, const DebugTunable* tunables, int count)
{
    auto fmtRow = [](const char* name, float v, int dec) {
        juce::String s;
        s << "  " << name;
        while (s.length() < 24) s << " ";
        s << juce::String(v, dec);
        return s;
    };
    for (int i = 0; i < count; i++)
        labels[i].setText(fmtRow(tunables[i].name, *tunables[i].value, tunables[i].decimals), juce::dontSendNotification);
}

// ==========================================================================

DebugTuningPanel::DebugTuningPanel(juce::ValueTree& state)
    : state(state)
{
    // --- Perspective section (data-driven) ---
    setupSectionHeader(perspectiveHeader, "Perspective");
    {
        float* ptrs[PERSP_COUNT] = {
            &PositionMath::debugPerspParamsGuitar.vanishingPointDepth,
            &PositionMath::debugPerspParamsGuitar.vanishingPointY,
            &PositionMath::debugPerspParamsGuitar.nearWidth,
            &PositionMath::debugPerspParamsGuitar.exponentialCurve,
            &PositionMath::debugPerspParamsGuitar.highwayDepth,
            &PositionMath::debugPerspParamsGuitar.playerDistance,
            &PositionMath::debugPerspParamsGuitar.perspectiveStrength
        };
        static constexpr const char* names[PERSP_COUNT] = {"VP Depth", "Horizon Y", "Fretboard Width", "Exp Curve", "Hwy Depth", "Player Dist", "Persp Str"};
        static constexpr float lo[PERSP_COUNT]   = {0.5f, -0.500f, 0.30f, 0.05f, 10.0f, 10.0f, 0.0f};
        static constexpr float hi[PERSP_COUNT]   = {20.0f, 0.500f, 2.00f, 2.0f, 500.0f, 500.0f, 2.0f};
        static constexpr float steps[PERSP_COUNT] = {0.1f, 0.001f, 0.005f, 0.01f, 5.0f, 5.0f, 0.01f};
        static constexpr int   dec[PERSP_COUNT]   = {1, 3, 3, 2, 0, 0, 2};
        static constexpr bool feat[PERSP_COUNT]  = {false, true, true, true, false, false, false};

        for (int i = 0; i < PERSP_COUNT; i++)
            perspTunables[i] = {names[i], ptrs[i], lo[i], hi[i], steps[i], dec[i], feat[i]};

        initTunableSliders(perspLabels, perspTunables, PERSP_COUNT,
                           [this]() { if (onPerspectiveChanged) onPerspectiveChanged(); });
    }

    // --- Track section (layers table + tiling + individual sliders) ---
    setupSectionHeader(trackHeader, "Track");

    for (int c = 0; c < LAYER_COLS; c++)
        setupTableHeader(layerColHdrLabels[c]);

    auto getLayerPtr = [this](int displayRow, int c) -> float* {
        int storageIdx = displayLayerMap[displayRow];
        switch (c) {
        case 0: return &layerStates[storageIdx].scale;
        case 1: return &layerStates[storageIdx].xOffset;
        default: return &layerStates[storageIdx].yOffset;
        }
    };

    static constexpr float layerStep[LAYER_COLS] = {0.005f, 0.0005f, 0.0005f};
    static constexpr float layerLo[LAYER_COLS]   = {0.10f, -1.0f, -1.0f};
    static constexpr float layerHi[LAYER_COLS]   = {3.00f,  1.0f,  1.0f};
    static constexpr int   layerDec[LAYER_COLS]   = {3, 4, 4};

    for (int r = 0; r < NUM_DISPLAY_LAYERS; r++)
    {
        layerRowLabels[r].setText(layerNames[r], juce::dontSendNotification);
        layerRowLabels[r].setJustificationType(juce::Justification::centredLeft);
        layerRowLabels[r].setColour(juce::Label::textColourId, juce::Colours::white);
        layerRowLabels[r].setFont(juce::Font(10.0f));

        for (int c = 0; c < LAYER_COLS; c++)
        {
            setupScrollLabel(layerParams[r][c]);
            layerParams[r][c].setFont(juce::Font(10.0f));
            layerParams[r][c].setJustificationType(juce::Justification::centred);
            auto apply = [this, r, c, getLayerPtr](float newVal) {
                float* val = getLayerPtr(r, c);
                *val = juce::jlimit(layerLo[c], layerHi[c], newVal);
                layerParams[r][c].setText(juce::String(*val, layerDec[c]), juce::dontSendNotification);
                fireLayer(r);
            };
            layerParams[r][c].onScroll = [c, getLayerPtr, r, apply](int delta) {
                apply(*getLayerPtr(r, c) + delta * layerStep[c]);
            };
            layerParams[r][c].onSet = apply;
        }
    }

    // Track section individual sliders (data-driven with per-slider callbacks)
    {
        static constexpr const char* names[TRACK_SLIDER_COUNT] = {
            "Step", "Scale", "Tex S", "Tex O",
            "Hwy Gtr", "Hwy Drm", "LogoGap", "DotNudge", "Grid"
        };
        float* ptrs[TRACK_SLIDER_COUNT] = {
            &tileStepValue, &tileScaleStepValue, &textureScaleValue, &textureOpacityValue,
            &hwyScaleGuitarValue, &hwyScaleDrumsValue, &logoPadValue, &dotNudgeValue, &gridlinePosOffset
        };
        static constexpr float lo[TRACK_SLIDER_COUNT]   = {0.30f, 0.30f, 0.10f, 0.05f, 0.50f, 0.50f, -0.1f, -0.5f, -0.10f};
        static constexpr float hi[TRACK_SLIDER_COUNT]   = {1.00f, 1.00f, 5.00f, 1.00f, 2.00f, 2.00f, 0.5f, 0.3f, 0.10f};
        static constexpr float steps[TRACK_SLIDER_COUNT] = {0.005f, 0.005f, 0.05f, 0.02f, 0.02f, 0.02f, 0.01f, 0.01f, 0.002f};
        static constexpr int   dec[TRACK_SLIDER_COUNT]   = {3, 3, 2, 2, 2, 2, 3, 3, 3};

        for (int i = 0; i < TRACK_SLIDER_COUNT; i++)
            trackSliderTunables[i] = {names[i], ptrs[i], lo[i], hi[i], steps[i], dec[i]};

        // Per-slider callbacks mapped by index
        std::function<void()> callbacks[TRACK_SLIDER_COUNT] = {
            [this]() { if (onTileStepChanged) onTileStepChanged(tileStepValue); },
            [this]() { if (onTileScaleStepChanged) onTileScaleStepChanged(tileScaleStepValue); },
            [this]() { if (onTextureScaleChanged) onTextureScaleChanged(textureScaleValue); },
            [this]() { if (onTextureOpacityChanged) onTextureOpacityChanged(textureOpacityValue); },
            [this]() { bemaniConfig.hwyScaleGuitar = hwyScaleGuitarValue; if (onHwyScaleChanged) onHwyScaleChanged(hwyScaleGuitarValue, hwyScaleDrumsValue); },
            [this]() { bemaniConfig.hwyScaleDrums = hwyScaleDrumsValue; if (onHwyScaleChanged) onHwyScaleChanged(hwyScaleGuitarValue, hwyScaleDrumsValue); },
            [this]() { if (onLogoPadChanged) onLogoPadChanged(logoPadValue, dotNudgeValue); },
            [this]() { if (onLogoPadChanged) onLogoPadChanged(logoPadValue, dotNudgeValue); },
            [this]() { fireChanged(); }
        };

        for (int i = 0; i < TRACK_SLIDER_COUNT; i++)
        {
            const auto& t = trackSliderTunables[i];
            setupScrollLabel(trackSliderLabels[i]);
            trackSliderLabels[i].setText(juce::String(t.name) + ": " + juce::String(*t.value, t.decimals), juce::dontSendNotification);
            auto apply = [this, i, cb = callbacks[i]](float newVal) {
                const auto& t = trackSliderTunables[i];
                *t.value = juce::jlimit(t.min, t.max, newVal);
                trackSliderLabels[i].setText(juce::String(t.name) + ": " + juce::String(*t.value, t.decimals), juce::dontSendNotification);
                if (cb) cb();
            };
            trackSliderLabels[i].onScroll = [this, i, apply](int delta) {
                const auto& t = trackSliderTunables[i];
                apply(*t.value + delta * t.step);
            };
            trackSliderLabels[i].onSet = apply;
        }
    }

    polyShadeToggle.setButtonText("Poly Shade");
    polyShadeToggle.onClick = [this]() {
        PositionMath::debugPolyShade = polyShadeToggle.getToggleState();
        if (onPolyShadeChanged) onPolyShadeChanged();
    };

    debugColourToggle.setButtonText("BG Colour");
    debugColourToggle.onClick = [this]() {
        if (onDebugColourChanged) onDebugColourChanged(debugColourToggle.getToggleState());
    };

    clickZonesToggle.setButtonText("Click Zones");
    clickZonesToggle.onClick = [this]() {
        if (onClickZonesChanged) onClickZonesChanged(clickZonesToggle.getToggleState());
    };

    stretchToggle.setButtonText("Stretch");
    stretchToggle.onClick = [this]() {
        if (onStretchChanged) onStretchChanged(stretchToggle.getToggleState());
    };

    bemaniToggle.setButtonText("Bemani");
    bemaniToggle.onClick = [this]() {
        if (onBemaniToggled) onBemaniToggled(bemaniToggle.getToggleState());
    };

    // --- Bemani section (data-driven from bemaniTunables) ---
    setupSectionHeader(bemaniHeader, "Bemani");

    auto setupSubHeader = [](juce::Label& lbl, const juce::String& text) {
        lbl.setText(text, juce::dontSendNotification);
        lbl.setJustificationType(juce::Justification::centredLeft);
        lbl.setColour(juce::Label::textColourId, juce::Colour(0xFFFF6B6B));
        lbl.setFont(juce::Font(12.0f, juce::Font::bold));
    };
    setupSubHeader(bemaniGroupHeaders[0], "Position");
    setupSubHeader(bemaniGroupHeaders[1], "Sustains");
    setupSubHeader(bemaniGroupHeaders[2], "Visual");

    for (int i = 0; i < BEMANI_TUNABLE_COUNT; i++)
    {
        const auto& t = bemaniTunables[i];
        setupScrollLabel(bemaniLabels[i]);
        if (t.featured)
        {
            bemaniLabels[i].setFont(juce::Font(13.0f).boldened());
            bemaniLabels[i].setColour(juce::Label::textColourId, juce::Colour(0xFF4FC3F7));
        }
        float& val = bemaniConfig.*t.field;
        bemaniLabels[i].setText(juce::String(t.name) + ": " + juce::String(val, t.decimals), juce::dontSendNotification);
        auto apply = [this, i, &t = bemaniTunables[i]](float newVal) {
            float& v = bemaniConfig.*t.field;
            v = juce::jlimit(t.min, t.max, newVal);
            bemaniLabels[i].setText(juce::String(t.name) + ": " + juce::String(v, t.decimals), juce::dontSendNotification);
            if (onBemaniTuningChanged) onBemaniTuningChanged();
        };
        bemaniLabels[i].onScroll = [this, &t = bemaniTunables[i], apply](int delta) {
            apply(bemaniConfig.*t.field + delta * t.step);
        };
        bemaniLabels[i].onSet = apply;
    }

    // --- Curvature section (data-driven) ---
    setupSectionHeader(curvatureHeader, "Curvature");
    curvatureHeader.setExpanded(true);
    {
        static constexpr const char* names[CURVATURE_COUNT] = {"Guitar", "Drums"};
        float* ptrs[CURVATURE_COUNT] = {&guitarCurvature, &drumCurvature};
        static constexpr float lo[CURVATURE_COUNT]   = {-0.20f, -0.20f};
        static constexpr float hi[CURVATURE_COUNT]   = {0.20f, 0.20f};
        static constexpr float steps[CURVATURE_COUNT] = {0.002f, 0.002f};
        static constexpr int   dec[CURVATURE_COUNT]   = {3, 3};

        for (int i = 0; i < CURVATURE_COUNT; i++)
            curvatureTunables[i] = {names[i], ptrs[i], lo[i], hi[i], steps[i], dec[i]};

        initTunableSliders(curvatureLabels, curvatureTunables, CURVATURE_COUNT,
                           [this]() { fireChanged(); });
    }

    // --- Base Scale table (Gem/Bar x W/H) ---
    setupSectionHeader(baseScaleHeader, "Base Scale");

    for (int c = 0; c < BASE_SCALE_COLS; c++)
        setupTableHeader(baseScaleColHdrLabels[c]);

    auto getBaseScalePtr = [this](int r, int c) -> float* {
        auto* s = (r == 0)
            ? (panelIsDrums ? &drumGemScale : &guitarGemScale)
            : (panelIsDrums ? &drumBarScale : &guitarBarScale);
        return (c == 0) ? &s->width : &s->height;
    };

    for (int r = 0; r < BASE_SCALE_ROWS; r++)
    {
        baseScaleRowLabels[r].setText(baseScaleRowNames[r], juce::dontSendNotification);
        baseScaleRowLabels[r].setJustificationType(juce::Justification::centredLeft);
        baseScaleRowLabels[r].setColour(juce::Label::textColourId, juce::Colours::white);
        baseScaleRowLabels[r].setFont(juce::Font(10.0f));

        for (int c = 0; c < BASE_SCALE_COLS; c++)
        {
            setupScrollLabel(baseScaleParams[r][c]);
            baseScaleParams[r][c].setFont(juce::Font(10.0f));
            baseScaleParams[r][c].setJustificationType(juce::Justification::centred);
            auto apply = [this, r, c, getBaseScalePtr](float newVal) {
                float* val = getBaseScalePtr(r, c);
                *val = juce::jlimit(0.50f, 2.00f, newVal);
                baseScaleParams[r][c].setText(juce::String(*val, 2), juce::dontSendNotification);
                fireChanged();
            };
            baseScaleParams[r][c].onScroll = [r, c, getBaseScalePtr, apply](int delta) {
                apply(*getBaseScalePtr(r, c) + delta * 0.01f);
            };
            baseScaleParams[r][c].onSet = apply;
        }
    }

    // Gem type scales (data-driven)
    {
        static constexpr const char* names[GEM_TYPE_COUNT] = {"Normal", "Cymbal", "HOPO", "GTap", "DGho", "DAcc", "CGho", "CAcc", "SPGem", "SPBar"};
        float* ptrs[GEM_TYPE_COUNT] = {
            &gemTypeScales.normal, &gemTypeScales.cymbal, &gemTypeScales.hopo, &gemTypeScales.gTap,
            &gemTypeScales.dGhost, &gemTypeScales.dAccent, &gemTypeScales.cGhost,
            &gemTypeScales.cAccent, &gemTypeScales.spGem, &gemTypeScales.spBar
        };
        for (int i = 0; i < GEM_TYPE_COUNT; i++)
            gemTypeTunables[i] = {names[i], ptrs[i], 0.30f, 2.00f, 0.01f, 2};

        initTunableSliders(gemTypeLabels, gemTypeTunables, GEM_TYPE_COUNT,
                           [this]() { fireChanged(); });
    }

    // --- Hit Scale table (Gem/Bar x S/W/H) ---
    setupSectionHeader(hitScaleHeader, "Hit Scale");

    for (int c = 0; c < HIT_SCALE_COLS; c++)
        setupTableHeader(hitScaleColHdrLabels[c]);

    auto getHitScalePtr = [this](int r, int c) -> float* {
        auto* s = (r == 0) ? &hitGemScale : &hitBarScale;
        switch (c) {
        case 0: return &s->scale;
        case 1: return &s->width;
        default: return &s->height;
        }
    };

    for (int r = 0; r < HIT_SCALE_ROWS; r++)
    {
        hitScaleRowLabels[r].setText(hitScaleRowNames[r], juce::dontSendNotification);
        hitScaleRowLabels[r].setJustificationType(juce::Justification::centredLeft);
        hitScaleRowLabels[r].setColour(juce::Label::textColourId, juce::Colours::white);
        hitScaleRowLabels[r].setFont(juce::Font(10.0f));

        for (int c = 0; c < HIT_SCALE_COLS; c++)
        {
            setupScrollLabel(hitScaleParams[r][c]);
            hitScaleParams[r][c].setFont(juce::Font(10.0f));
            hitScaleParams[r][c].setJustificationType(juce::Justification::centred);
            auto apply = [this, r, c, getHitScalePtr](float newVal) {
                float* val = getHitScalePtr(r, c);
                *val = juce::jlimit(0.50f, 3.00f, newVal);
                hitScaleParams[r][c].setText(juce::String(*val, 2), juce::dontSendNotification);
                fireChanged();
            };
            hitScaleParams[r][c].onScroll = [r, c, getHitScalePtr, apply](int delta) {
                apply(*getHitScalePtr(r, c) + delta * 0.01f);
            };
            hitScaleParams[r][c].onSet = apply;
        }
    }

    // Hit type scales (data-driven)
    {
        static constexpr const char* names[HIT_TYPE_FLOAT_COUNT] = {"Ghost", "Accent", "HOPO", "Tap", "SP"};
        float* ptrs[HIT_TYPE_FLOAT_COUNT] = {
            &hitTypeConfig.ghost, &hitTypeConfig.accent, &hitTypeConfig.hopo,
            &hitTypeConfig.tap, &hitTypeConfig.sp
        };
        static constexpr float lo[HIT_TYPE_FLOAT_COUNT] = {0.30f, 0.50f, 0.30f, 0.30f, 0.50f};
        static constexpr float hi[HIT_TYPE_FLOAT_COUNT] = {1.50f, 2.00f, 2.00f, 1.50f, 2.00f};

        for (int i = 0; i < HIT_TYPE_FLOAT_COUNT; i++)
            hitTypeTunables[i] = {names[i], ptrs[i], lo[i], hi[i], 0.01f, 2};

        initTunableSliders(hitTypeLabels, hitTypeTunables, HIT_TYPE_FLOAT_COUNT,
                           [this]() { fireChanged(); });
    }

    // Hit type bools (backed by hitTypeConfig struct)
    {
        static constexpr const char* boolNames[HIT_TYPE_BOOL_COUNT] = {"SP White: ", "Tap Purple:"};
        bool* boolPtrs[HIT_TYPE_BOOL_COUNT] = {&hitTypeConfig.spWhiteFlare, &hitTypeConfig.tapPurpleFlare};
        for (int i = 0; i < HIT_TYPE_BOOL_COUNT; i++)
        {
            setupScrollLabel(hitTypeBoolLabels[i]);
            hitTypeBoolLabels[i].onScroll = [this, i, boolPtrs](int) {
                *boolPtrs[i] = !*boolPtrs[i];
                hitTypeBoolLabels[i].setText(juce::String(boolNames[i]) + (*boolPtrs[i] ? "ON" : "OFF"), juce::dontSendNotification);
                fireChanged();
            };
        }
    }

    // --- Z Offsets table (6 rows x 2 cols: Guitar/Drums) ---
    setupSectionHeader(zOffsetsHeader, "Z Offsets");
    zOffsetsHeader.setExpanded(true);  // most-touched section — show by default

    for (int c = 0; c < Z_COLS; c++)
        setupTableHeader(zColHdrLabels[c]);

    auto getZPtr = [this](int r, int c) -> float* {
        auto* o = (c == 0) ? &guitarOffsets : &drumOffsets;
        switch (r) {
        case 0: return &o->gridZ;
        case 1: return &o->gemZ;
        case 2: return &o->cymZ;   // Drums only; guitar cymZ is dead weight
        case 3: return &o->barZ;
        case 4: return &o->hitGemZ;
        default: return &o->hitBarZ;
        }
    };

    for (int r = 0; r < Z_ROWS; r++)
    {
        zRowLabels[r].setText(zRowNames[r], juce::dontSendNotification);
        zRowLabels[r].setJustificationType(juce::Justification::centredLeft);
        zRowLabels[r].setColour(juce::Label::textColourId, juce::Colours::white);
        zRowLabels[r].setFont(juce::Font(13.0f).boldened());

        for (int c = 0; c < Z_COLS; c++)
        {
            setupScrollLabel(zParams[r][c]);
            zParams[r][c].setFont(juce::Font(13.0f));
            zParams[r][c].setJustificationType(juce::Justification::centred);
            auto apply = [this, r, c, getZPtr](float newVal) {
                float* val = getZPtr(r, c);
                *val = juce::jlimit(-50.0f, 50.0f, newVal);
                zParams[r][c].setText(juce::String(*val, 1), juce::dontSendNotification);
                fireChanged();
            };
            zParams[r][c].onScroll = [r, c, getZPtr, apply](int delta) {
                apply(*getZPtr(r, c) + delta * 0.5f);
            };
            zParams[r][c].onSet = apply;
        }
    }

    // --- Strike Position table (2 rows x 2 cols: Guitar/Drums) ---
    setupSectionHeader(strikeHeader, "Strike Pos");

    for (int c = 0; c < STRIKE_COLS; c++)
        setupTableHeader(strikeColHdrLabels[c]);

    auto getStrikePtr = [this](int r, int c) -> float* {
        auto* o = (c == 0) ? &guitarOffsets : &drumOffsets;
        return (r == 0) ? &o->strikePosGem : &o->strikePosBar;
    };

    for (int r = 0; r < STRIKE_ROWS; r++)
    {
        strikeRowLabels[r].setText(strikeRowNames[r], juce::dontSendNotification);
        strikeRowLabels[r].setJustificationType(juce::Justification::centredLeft);
        strikeRowLabels[r].setColour(juce::Label::textColourId, juce::Colours::white);
        strikeRowLabels[r].setFont(juce::Font(10.0f));

        for (int c = 0; c < STRIKE_COLS; c++)
        {
            setupScrollLabel(strikeParams[r][c]);
            strikeParams[r][c].setFont(juce::Font(10.0f));
            strikeParams[r][c].setJustificationType(juce::Justification::centred);
            auto apply = [this, r, c, getStrikePtr](float newVal) {
                float* val = getStrikePtr(r, c);
                *val = juce::jlimit(-0.10f, 0.10f, newVal);
                strikeParams[r][c].setText(juce::String(*val, 3), juce::dontSendNotification);
                fireChanged();
            };
            strikeParams[r][c].onScroll = [r, c, getStrikePtr, apply](int delta) {
                apply(*getStrikePtr(r, c) + delta * 0.002f);
            };
            strikeParams[r][c].onSet = apply;
        }
    }

    // Helper: setup a sub-section label ("Notes" / "Lanes")
    auto setupSubLabel = [](juce::Label& label, const juce::String& text) {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setColour(juce::Label::textColourId, juce::Colours::grey);
        label.setFont(juce::Font(10.0f));
    };

    // Helper: get ColumnAdjust field by column index (shared by guitar/drums note tables)
    // Columns: 0=Z, 1=S1, 2=S2, 3=W, 4=H
    auto getColAdjustField = [](PositionConstants::ColumnAdjust& ca, int c) -> float* {
        switch (c) {
        case 0: return &ca.z;
        case 1: return &ca.sNear;
        case 2: return &ca.sFar;
        case 3: return &ca.w;
        default: return &ca.h;
        }
    };

    // Note table scroll params per column: {step, lo, hi, decimals}
    // Z is pixel offset, S1/S2/W/H are scale multipliers
    static constexpr float noteStep[COL_NOTE_COLS] = {0.5f, 0.01f, 0.01f, 0.01f, 0.01f};
    static constexpr float noteLo[COL_NOTE_COLS]   = {-30.f, 0.30f, 0.30f, 0.30f, 0.30f};
    static constexpr float noteHi[COL_NOTE_COLS]   = {30.f, 3.00f, 3.00f, 3.00f, 3.00f};
    static constexpr int   noteDec[COL_NOTE_COLS]   = {1, 2, 2, 2, 2};

    // --- Guitar Cols section ---
    setupSectionHeader(guitarColsHeader, "Guitar Cols");
    setupSubLabel(gcolSubNoteLabel, "Notes");
    setupSubLabel(gcolSubLaneLabel, "Lanes");

    // Guitar notes sub-table (6 rows x 7 cols)
    for (int c = 0; c < COL_NOTE_COLS; c++)
        setupTableHeader(gcolNoteHdrLabels[c]);

    for (int r = 0; r < GUITAR_LANES; r++)
    {
        gcolNoteRowLabels[r].setText(guitarColNames[r], juce::dontSendNotification);
        gcolNoteRowLabels[r].setJustificationType(juce::Justification::centredLeft);
        gcolNoteRowLabels[r].setColour(juce::Label::textColourId, juce::Colours::white);
        gcolNoteRowLabels[r].setFont(juce::Font(10.0f));

        for (int c = 0; c < COL_NOTE_COLS; c++)
        {
            setupScrollLabel(gcolNoteParams[r][c]);
            gcolNoteParams[r][c].setFont(juce::Font(10.0f));
            gcolNoteParams[r][c].setJustificationType(juce::Justification::centred);
            auto apply = [this, r, c, getColAdjustField](float newVal) {
                float* val = getColAdjustField(guitarColAdjust[r], c);
                *val = juce::jlimit(noteLo[c], noteHi[c], newVal);
                gcolNoteParams[r][c].setText(juce::String(*val, noteDec[c]), juce::dontSendNotification);
                fireChanged();
            };
            gcolNoteParams[r][c].onScroll = [this, r, c, getColAdjustField, apply](int delta) {
                apply(*getColAdjustField(guitarColAdjust[r], c) + delta * noteStep[c]);
            };
            gcolNoteParams[r][c].onSet = apply;
        }
    }

    // Guitar lanes sub-table (6 rows x 4 cols)
    std::copy_n(PositionConstants::guitarBezierLaneCoords, GUITAR_LANES, guitarLaneCoords);

    for (int c = 0; c < COL_LANE_COLS; c++)
        setupTableHeader(gcolLaneHdrLabels[c]);

    auto getGcolLanePtr = [this](int r, int c) -> float* {
        switch (c) {
        case 0: return &guitarLaneCoords[r].normX1;
        default: return &guitarLaneCoords[r].normWidth1;
        }
    };

    for (int r = 0; r < GUITAR_LANES; r++)
    {
        gcolLaneRowLabels[r].setText(guitarColNames[r], juce::dontSendNotification);
        gcolLaneRowLabels[r].setJustificationType(juce::Justification::centredLeft);
        gcolLaneRowLabels[r].setColour(juce::Label::textColourId, juce::Colours::white);
        gcolLaneRowLabels[r].setFont(juce::Font(10.0f));

        for (int c = 0; c < COL_LANE_COLS; c++)
        {
            setupScrollLabel(gcolLaneParams[r][c]);
            gcolLaneParams[r][c].setFont(juce::Font(10.0f));
            gcolLaneParams[r][c].setJustificationType(juce::Justification::centred);
            auto apply = [this, r, c, getGcolLanePtr](float newVal) {
                float* val = getGcolLanePtr(r, c);
                *val = juce::jlimit(0.0f, 1.0f, newVal);
                gcolLaneParams[r][c].setText(juce::String(*val, 3), juce::dontSendNotification);
                fireLaneChanged();
            };
            gcolLaneParams[r][c].onScroll = [r, c, getGcolLanePtr, apply](int delta) {
                apply(*getGcolLanePtr(r, c) + delta * 0.001f);
            };
            gcolLaneParams[r][c].onSet = apply;
        }
    }

    // --- Drum Cols section ---
    setupSectionHeader(drumColsHeader, "Drum Cols");
    setupSubLabel(dcolSubNoteLabel, "Notes");
    setupSubLabel(dcolSubLaneLabel, "Lanes");

    // Drum notes sub-table (5 rows x 7 cols)
    for (int c = 0; c < COL_NOTE_COLS; c++)
        setupTableHeader(dcolNoteHdrLabels[c]);

    for (int r = 0; r < DRUM_LANES; r++)
    {
        dcolNoteRowLabels[r].setText(drumColNames[r], juce::dontSendNotification);
        dcolNoteRowLabels[r].setJustificationType(juce::Justification::centredLeft);
        dcolNoteRowLabels[r].setColour(juce::Label::textColourId, juce::Colours::white);
        dcolNoteRowLabels[r].setFont(juce::Font(10.0f));

        for (int c = 0; c < COL_NOTE_COLS; c++)
        {
            setupScrollLabel(dcolNoteParams[r][c]);
            dcolNoteParams[r][c].setFont(juce::Font(10.0f));
            dcolNoteParams[r][c].setJustificationType(juce::Justification::centred);
            auto apply = [this, r, c, getColAdjustField](float newVal) {
                float* val = getColAdjustField(drumColAdjust[r], c);
                *val = juce::jlimit(noteLo[c], noteHi[c], newVal);
                dcolNoteParams[r][c].setText(juce::String(*val, noteDec[c]), juce::dontSendNotification);
                fireChanged();
            };
            dcolNoteParams[r][c].onScroll = [this, r, c, getColAdjustField, apply](int delta) {
                apply(*getColAdjustField(drumColAdjust[r], c) + delta * noteStep[c]);
            };
            dcolNoteParams[r][c].onSet = apply;
        }
    }

    // Drum lanes sub-table (5 rows x 4 cols)
    std::copy_n(PositionConstants::drumBezierLaneCoords, DRUM_LANES, drumLaneCoords);

    for (int c = 0; c < COL_LANE_COLS; c++)
        setupTableHeader(dcolLaneHdrLabels[c]);

    auto getDcolLanePtr = [this](int r, int c) -> float* {
        switch (c) {
        case 0: return &drumLaneCoords[r].normX1;
        default: return &drumLaneCoords[r].normWidth1;
        }
    };

    for (int r = 0; r < DRUM_LANES; r++)
    {
        dcolLaneRowLabels[r].setText(drumColNames[r], juce::dontSendNotification);
        dcolLaneRowLabels[r].setJustificationType(juce::Justification::centredLeft);
        dcolLaneRowLabels[r].setColour(juce::Label::textColourId, juce::Colours::white);
        dcolLaneRowLabels[r].setFont(juce::Font(10.0f));

        for (int c = 0; c < COL_LANE_COLS; c++)
        {
            setupScrollLabel(dcolLaneParams[r][c]);
            dcolLaneParams[r][c].setFont(juce::Font(10.0f));
            dcolLaneParams[r][c].setJustificationType(juce::Justification::centred);
            auto apply = [this, r, c, getDcolLanePtr](float newVal) {
                float* val = getDcolLanePtr(r, c);
                *val = juce::jlimit(0.0f, 1.0f, newVal);
                dcolLaneParams[r][c].setText(juce::String(*val, 3), juce::dontSendNotification);
                fireLaneChanged();
            };
            dcolLaneParams[r][c].onScroll = [r, c, getDcolLanePtr, apply](int delta) {
                apply(*getDcolLanePtr(r, c) + delta * 0.001f);
            };
            dcolLaneParams[r][c].onSet = apply;
        }
    }

    // --- Lane Width sliders ---
    static const auto laneMonoFont = juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain);
    auto fmtLane = [](const char* name, float v) {
        juce::String s;
        s << "  " << name;
        while (s.length() < 24) s << " ";
        s << juce::String(v, 2);
        return s;
    };

    setupScrollLabel(laneWidthLabel);
    laneWidthLabel.setFont(laneMonoFont);
    laneWidthLabel.setText(fmtLane("Lane Width", PositionConstants::LANE_WIDTH), juce::dontSendNotification);
    auto applyLaneWidth = [this, fmtLane](float newVal) {
        PositionConstants::LANE_WIDTH = juce::jlimit(0.20f, 2.00f, newVal);
        laneWidthLabel.setText(fmtLane("Lane Width", PositionConstants::LANE_WIDTH), juce::dontSendNotification);
        fireChanged();
    };
    laneWidthLabel.onScroll = [applyLaneWidth](int delta) {
        applyLaneWidth(PositionConstants::LANE_WIDTH + delta * 0.02f);
    };
    laneWidthLabel.onSet = applyLaneWidth;

    setupScrollLabel(laneOpenWidthLabel);
    laneOpenWidthLabel.setFont(laneMonoFont);
    laneOpenWidthLabel.setText(fmtLane("Lane Open Width", PositionConstants::LANE_OPEN_WIDTH), juce::dontSendNotification);
    auto applyLaneOpenWidth = [this, fmtLane](float newVal) {
        PositionConstants::LANE_OPEN_WIDTH = juce::jlimit(0.20f, 2.00f, newVal);
        laneOpenWidthLabel.setText(fmtLane("Lane Open Width", PositionConstants::LANE_OPEN_WIDTH), juce::dontSendNotification);
        fireChanged();
    };
    laneOpenWidthLabel.onScroll = [applyLaneOpenWidth](int delta) {
        applyLaneOpenWidth(PositionConstants::LANE_OPEN_WIDTH + delta * 0.02f);
    };
    laneOpenWidthLabel.onSet = applyLaneOpenWidth;

    // --- Lane Shape section ---
    setupSectionHeader(laneShapeHeader, "Lane Shape");
    for (int c = 0; c < LANE_SHAPE_COLS; c++)
        setupTableHeader(laneShapeColHdrLabels[c]);

    auto getLaneShapePtr = [this](int r, int c) -> float* {
        if (r == 0) {
            switch (c) {
            case 0: return &laneShape.startOffset;
            case 1: return &laneShape.innerStartArc;
            default: return &laneShape.outerStartArc;
            }
        } else {
            switch (c) {
            case 0: return &laneShape.endOffset;
            case 1: return &laneShape.innerEndArc;
            default: return &laneShape.outerEndArc;
            }
        }
    };

    for (int r = 0; r < LANE_SHAPE_ROWS; r++)
    {
        laneShapeRowLabels[r].setText(laneShapeRowNames[r], juce::dontSendNotification);
        laneShapeRowLabels[r].setJustificationType(juce::Justification::centredLeft);
        laneShapeRowLabels[r].setColour(juce::Label::textColourId, juce::Colours::white);
        laneShapeRowLabels[r].setFont(juce::Font(10.0f));
        for (int c = 0; c < LANE_SHAPE_COLS; c++)
        {
            setupScrollLabel(laneShapeParams[r][c]);
            laneShapeParams[r][c].setFont(juce::Font(10.0f));
            laneShapeParams[r][c].setJustificationType(juce::Justification::centred);
            auto apply = [this, r, c, getLaneShapePtr](float newVal) {
                float lo = (c == 0) ? -0.10f : -0.20f;
                float hi = (c == 0) ?  0.10f :  0.20f;
                float* val = getLaneShapePtr(r, c);
                *val = juce::jlimit(lo, hi, newVal);
                laneShapeParams[r][c].setText(juce::String(*val, 3), juce::dontSendNotification);
                fireChanged();
            };
            laneShapeParams[r][c].onScroll = [r, c, getLaneShapePtr, apply](int delta) {
                float step = (c == 0) ? 0.002f : 0.005f;
                apply(*getLaneShapePtr(r, c) + delta * step);
            };
            laneShapeParams[r][c].onSet = apply;
        }
    }

    // --- Overlay Adjust section ---
    setupSectionHeader(overlayAdjustHeader, "Overlay Adjust");
    std::copy_n(PositionConstants::OVERLAY_DEFAULTS, NUM_OVERLAY_TYPES, overlayAdjusts);

    for (int c = 0; c < OVERLAY_PARAMS; c++)
        setupTableHeader(overlayColHeaderLabels[c]);

    for (int r = 0; r < NUM_OVERLAY_TYPES; r++)
    {
        overlayRowNameLabels[r].setText(overlayRowNames[r], juce::dontSendNotification);
        overlayRowNameLabels[r].setJustificationType(juce::Justification::centredLeft);
        overlayRowNameLabels[r].setColour(juce::Label::textColourId, juce::Colours::white);
        overlayRowNameLabels[r].setFont(juce::Font(10.0f));

        for (int c = 0; c < OVERLAY_PARAMS; c++)
        {
            setupScrollLabel(overlayParamLabels[r][c]);
            overlayParamLabels[r][c].setFont(juce::Font(10.0f));
            overlayParamLabels[r][c].setJustificationType(juce::Justification::centred);
            auto getOverlayPtr = [this, r, c]() -> float* {
                switch (c) {
                case 0: return &overlayAdjusts[r].offsetX;
                case 1: return &overlayAdjusts[r].offsetY;
                case 2: return &overlayAdjusts[r].scaleX;
                case 3: return &overlayAdjusts[r].scaleY;
                default: return &overlayAdjusts[r].scale;
                }
            };
            auto apply = [this, r, c, getOverlayPtr](float newVal) {
                float lo = (c < 2) ? -1.0f : 0.10f;
                float hi = (c < 2) ?  1.0f : 3.00f;
                float* val = getOverlayPtr();
                *val = juce::jlimit(lo, hi, newVal);
                overlayParamLabels[r][c].setText(juce::String(*val, 2), juce::dontSendNotification);
                fireChanged();
            };
            overlayParamLabels[r][c].onScroll = [getOverlayPtr, apply](int delta) {
                apply(*getOverlayPtr() + delta * 0.01f);
            };
            overlayParamLabels[r][c].onSet = apply;
        }
    }

    // --- Unified Adjust table ---
    setupSectionHeader(adjustHeader, "Adjust (X,Y=offset  W,H,S=scale)");
    adjustHeader.setExpanded(true);

    for (int c = 0; c < ADJUST_COLS; c++)
    {
        setupTableHeader(adjustColHdrLabels[c]);
        adjustColHdrLabels[c].setText(adjustColNames[c], juce::dontSendNotification);
    }

    for (int r = 0; r < ADJUST_ROWS; r++)
    {
        adjustRowNameLabels[r].setText(adjustRowNames[r], juce::dontSendNotification);
        adjustRowNameLabels[r].setJustificationType(juce::Justification::centredLeft);
        adjustRowNameLabels[r].setColour(juce::Label::textColourId, juce::Colours::white);
        adjustRowNameLabels[r].setFont(juce::Font(10.0f));

        for (int c = 0; c < ADJUST_COLS; c++)
        {
            auto& cell = adjustParams[r][c];
            setupScrollLabel(cell);
            cell.setFont(juce::Font(10.0f));
            cell.setJustificationType(juce::Justification::centred);

            if (getAdjustPtr(r, c) == nullptr)
            {
                cell.setText("-", juce::dontSendNotification);
                cell.setColour(juce::Label::textColourId, juce::Colours::grey);
                cell.setEditable(false, false, false);
                cell.setWantsKeyboardFocus(false);
                cell.setInterceptsMouseClicks(false, false);
                continue;
            }

            auto apply = [this, r, c](float newVal) {
                float* val = getAdjustPtr(r, c);
                if (val == nullptr) return;
                // Offset rows get a tight range; scale cells use a wider one.
                const float lo = (c < 2) ? -1.0f : 0.10f;
                const float hi = (c < 2) ?  1.0f : 3.00f;
                *val = juce::jlimit(lo, hi, newVal);
                adjustParams[r][c].setText(juce::String(*val, 2), juce::dontSendNotification);
                fireChanged();
            };
            cell.onScroll = [this, r, c, apply](int delta) {
                float* val = getAdjustPtr(r, c);
                if (val == nullptr) return;
                apply(*val + delta * 0.01f);
            };
            cell.onSet = apply;
        }
    }

    refreshLabels();
    refreshTrackLabels();
    refreshColLabels();
    refreshAdjustLabels();

    // =========================================================================
    // Register panel children
    // =========================================================================

    // Perspective section
    tuningButton.addPanelChild(&perspectiveHeader);
    addTunableChildren(perspLabels, PERSP_COUNT);

    tuningButton.addPanelChild(&trackHeader);
    for (int c = 0; c < LAYER_COLS; c++)
        tuningButton.addPanelChild(&layerColHdrLabels[c]);
    for (int r = 0; r < NUM_DISPLAY_LAYERS; r++)
    {
        tuningButton.addPanelChild(&layerRowLabels[r]);
        for (int c = 0; c < LAYER_COLS; c++)
            tuningButton.addPanelChild(&layerParams[r][c]);
    }
    addTunableChildren(trackSliderLabels, TRACK_SLIDER_COUNT);
    tuningButton.addPanelChild(&polyShadeToggle);
    tuningButton.addPanelChild(&debugColourToggle);
    tuningButton.addPanelChild(&clickZonesToggle);
    tuningButton.addPanelChild(&stretchToggle);
    tuningButton.addPanelChild(&bemaniToggle);

    tuningButton.addPanelChild(&bemaniHeader);
    for (int g = 0; g < 3; g++)
        tuningButton.addPanelChild(&bemaniGroupHeaders[g]);
    for (int i = 0; i < BEMANI_TUNABLE_COUNT; i++)
        tuningButton.addPanelChild(&bemaniLabels[i]);

    tuningButton.addPanelChild(&curvatureHeader);
    addTunableChildren(curvatureLabels, CURVATURE_COUNT);

    // Base Scale table
    tuningButton.addPanelChild(&baseScaleHeader);
    for (int c = 0; c < BASE_SCALE_COLS; c++)
        tuningButton.addPanelChild(&baseScaleColHdrLabels[c]);
    for (int r = 0; r < BASE_SCALE_ROWS; r++)
    {
        tuningButton.addPanelChild(&baseScaleRowLabels[r]);
        for (int c = 0; c < BASE_SCALE_COLS; c++)
            tuningButton.addPanelChild(&baseScaleParams[r][c]);
    }
    addTunableChildren(gemTypeLabels, GEM_TYPE_COUNT);

    // Hit Scale table
    tuningButton.addPanelChild(&hitScaleHeader);
    for (int c = 0; c < HIT_SCALE_COLS; c++)
        tuningButton.addPanelChild(&hitScaleColHdrLabels[c]);
    for (int r = 0; r < HIT_SCALE_ROWS; r++)
    {
        tuningButton.addPanelChild(&hitScaleRowLabels[r]);
        for (int c = 0; c < HIT_SCALE_COLS; c++)
            tuningButton.addPanelChild(&hitScaleParams[r][c]);
    }
    addTunableChildren(hitTypeLabels, HIT_TYPE_FLOAT_COUNT);
    for (int i = 0; i < HIT_TYPE_BOOL_COUNT; i++)
        tuningButton.addPanelChild(&hitTypeBoolLabels[i]);

    // Z Offsets table
    tuningButton.addPanelChild(&zOffsetsHeader);
    for (int c = 0; c < Z_COLS; c++)
        tuningButton.addPanelChild(&zColHdrLabels[c]);
    for (int r = 0; r < Z_ROWS; r++)
    {
        tuningButton.addPanelChild(&zRowLabels[r]);
        for (int c = 0; c < Z_COLS; c++)
            tuningButton.addPanelChild(&zParams[r][c]);
    }

    // Strike Position table
    tuningButton.addPanelChild(&strikeHeader);
    for (int c = 0; c < STRIKE_COLS; c++)
        tuningButton.addPanelChild(&strikeColHdrLabels[c]);
    for (int r = 0; r < STRIKE_ROWS; r++)
    {
        tuningButton.addPanelChild(&strikeRowLabels[r]);
        for (int c = 0; c < STRIKE_COLS; c++)
            tuningButton.addPanelChild(&strikeParams[r][c]);
    }

    // Guitar Cols (notes + lanes)
    tuningButton.addPanelChild(&guitarColsHeader);
    tuningButton.addPanelChild(&gcolSubNoteLabel);
    for (int c = 0; c < COL_NOTE_COLS; c++)
        tuningButton.addPanelChild(&gcolNoteHdrLabels[c]);
    for (int r = 0; r < GUITAR_LANES; r++)
    {
        tuningButton.addPanelChild(&gcolNoteRowLabels[r]);
        for (int c = 0; c < COL_NOTE_COLS; c++)
            tuningButton.addPanelChild(&gcolNoteParams[r][c]);
    }
    tuningButton.addPanelChild(&gcolSubLaneLabel);
    for (int c = 0; c < COL_LANE_COLS; c++)
        tuningButton.addPanelChild(&gcolLaneHdrLabels[c]);
    for (int r = 0; r < GUITAR_LANES; r++)
    {
        tuningButton.addPanelChild(&gcolLaneRowLabels[r]);
        for (int c = 0; c < COL_LANE_COLS; c++)
            tuningButton.addPanelChild(&gcolLaneParams[r][c]);
    }

    // Drum Cols (notes + lanes)
    tuningButton.addPanelChild(&drumColsHeader);
    tuningButton.addPanelChild(&dcolSubNoteLabel);
    for (int c = 0; c < COL_NOTE_COLS; c++)
        tuningButton.addPanelChild(&dcolNoteHdrLabels[c]);
    for (int r = 0; r < DRUM_LANES; r++)
    {
        tuningButton.addPanelChild(&dcolNoteRowLabels[r]);
        for (int c = 0; c < COL_NOTE_COLS; c++)
            tuningButton.addPanelChild(&dcolNoteParams[r][c]);
    }
    tuningButton.addPanelChild(&dcolSubLaneLabel);
    for (int c = 0; c < COL_LANE_COLS; c++)
        tuningButton.addPanelChild(&dcolLaneHdrLabels[c]);
    for (int r = 0; r < DRUM_LANES; r++)
    {
        tuningButton.addPanelChild(&dcolLaneRowLabels[r]);
        for (int c = 0; c < COL_LANE_COLS; c++)
            tuningButton.addPanelChild(&dcolLaneParams[r][c]);
    }

    // Lane Width
    tuningButton.addPanelChild(&laneWidthLabel);
    tuningButton.addPanelChild(&laneOpenWidthLabel);

    // Lane Shape table
    tuningButton.addPanelChild(&laneShapeHeader);
    for (int c = 0; c < LANE_SHAPE_COLS; c++)
        tuningButton.addPanelChild(&laneShapeColHdrLabels[c]);
    for (int r = 0; r < LANE_SHAPE_ROWS; r++)
    {
        tuningButton.addPanelChild(&laneShapeRowLabels[r]);
        for (int c = 0; c < LANE_SHAPE_COLS; c++)
            tuningButton.addPanelChild(&laneShapeParams[r][c]);
    }

    // Overlay Adjust table (legacy — hidden in layoutPanel; retained so the
    // OverlayAdjust* data syncing doesn't require a reshuffle yet.)
    tuningButton.addPanelChild(&overlayAdjustHeader);
    for (int c = 0; c < OVERLAY_PARAMS; c++)
        tuningButton.addPanelChild(&overlayColHeaderLabels[c]);
    for (int r = 0; r < NUM_OVERLAY_TYPES; r++)
    {
        tuningButton.addPanelChild(&overlayRowNameLabels[r]);
        for (int c = 0; c < OVERLAY_PARAMS; c++)
            tuningButton.addPanelChild(&overlayParamLabels[r][c]);
    }

    // Unified Adjust table
    tuningButton.addPanelChild(&adjustHeader);
    for (int c = 0; c < ADJUST_COLS; c++)
        tuningButton.addPanelChild(&adjustColHdrLabels[c]);
    for (int r = 0; r < ADJUST_ROWS; r++)
    {
        tuningButton.addPanelChild(&adjustRowIcons[r]);
        tuningButton.addPanelChild(&adjustRowNameLabels[r]);
        for (int c = 0; c < ADJUST_COLS; c++)
            tuningButton.addPanelChild(&adjustParams[r][c]);
    }

    tuningButton.setPanelSize(280, 580);
    tuningButton.onLayoutPanel = [this](juce::Component* panel) { layoutPanel(panel); };
}

DebugTuningPanel::~DebugTuningPanel()
{
    tuningButton.dismissPanel();
}

void DebugTuningPanel::applyTo(SceneRenderer& sr) const
{
    sr.noteCurvatureGuitar = guitarCurvature;
    sr.noteCurvatureDrums = drumCurvature;
    sr.guitarGemScale = guitarGemScale;
    sr.drumGemScale   = drumGemScale;
    sr.guitarBarScale = guitarBarScale;
    sr.drumBarScale   = drumBarScale;
    sr.hitGemScale = hitGemScale;
    sr.hitBarScale = hitBarScale;
    sr.hitTypeConfig = hitTypeConfig;
    sr.gemTypeScales = gemTypeScales;

    sr.guitarOffsets = guitarOffsets;
    sr.drumOffsets = drumOffsets;

    std::copy_n(guitarColAdjust, 6, sr.guitarColAdjust);
    std::copy_n(drumColAdjust, 5, sr.drumColAdjust);

    std::copy_n(guitarLaneCoords, GUITAR_LANES, sr.guitarLaneCoordsLocal);
    std::copy_n(drumLaneCoords, DRUM_LANES, sr.drumLaneCoordsLocal);
    std::copy_n(overlayAdjusts, NUM_OVERLAY_TYPES, sr.overlayAdjusts);
    sr.gridlinePosOffset = gridlinePosOffset;
    sr.laneShape = laneShape;
}

void DebugTuningPanel::initDefaults(const TrackRenderer& trackRenderer)
{
    std::copy_n(trackRenderer.layersGuitar, NUM_TRACK_LAYERS, guitarStates);
    std::copy_n(trackRenderer.layersDrums, NUM_TRACK_LAYERS, drumStates);
    tileStepValue = trackRenderer.tileStep;
    tileScaleStepValue = trackRenderer.tileScaleStep;
    textureScaleValue = trackRenderer.textureScale;
    textureOpacityValue = trackRenderer.textureOpacity;
    refreshTrackLabels();
}

void DebugTuningPanel::setDrums(bool isDrums)
{
    layerStates = isDrums ? drumStates : guitarStates;
    panelIsDrums = isDrums;
    refreshTrackLabels();
    refreshAdjustLabels();   // Note/Bar W/H rows now route to the new instrument
}

void DebugTuningPanel::fireLayer(int displayIdx)
{
    int storageIdx = displayLayerMap[displayIdx];
    if (onLayerChanged)
        onLayerChanged(storageIdx, layerStates[storageIdx].scale, layerStates[storageIdx].xOffset, layerStates[storageIdx].yOffset);
}

void DebugTuningPanel::refreshTrackLabels()
{
    static constexpr int layerDec[LAYER_COLS] = {3, 4, 4};
    for (int c = 0; c < LAYER_COLS; c++)
        layerColHdrLabels[c].setText(layerColNames[c], juce::dontSendNotification);
    for (int r = 0; r < NUM_DISPLAY_LAYERS; r++)
    {
        int si = displayLayerMap[r];
        const float vals[LAYER_COLS] = {layerStates[si].scale, layerStates[si].xOffset, layerStates[si].yOffset};
        for (int c = 0; c < LAYER_COLS; c++)
            layerParams[r][c].setText(juce::String(vals[c], layerDec[c]), juce::dontSendNotification);
    }
    refreshTunableLabels(trackSliderLabels, trackSliderTunables, TRACK_SLIDER_COUNT);
    polyShadeToggle.setToggleState(PositionMath::debugPolyShade, juce::dontSendNotification);
}

void DebugTuningPanel::refreshColLabels()
{
    static constexpr int noteDec[COL_NOTE_COLS] = {1, 2, 2, 2, 2};

    // Guitar notes
    for (int c = 0; c < COL_NOTE_COLS; c++)
        gcolNoteHdrLabels[c].setText(colNoteColNames[c], juce::dontSendNotification);
    for (int r = 0; r < GUITAR_LANES; r++)
    {
        const auto& ca = guitarColAdjust[r];
        const float vals[COL_NOTE_COLS] = {ca.z, ca.sNear, ca.sFar, ca.w, ca.h};
        for (int c = 0; c < COL_NOTE_COLS; c++)
            gcolNoteParams[r][c].setText(juce::String(vals[c], noteDec[c]), juce::dontSendNotification);
    }

    // Guitar lanes
    for (int c = 0; c < COL_LANE_COLS; c++)
        gcolLaneHdrLabels[c].setText(colLaneColNames[c], juce::dontSendNotification);
    for (int r = 0; r < GUITAR_LANES; r++)
    {
        gcolLaneParams[r][0].setText(juce::String(guitarLaneCoords[r].normX1, 3), juce::dontSendNotification);
        gcolLaneParams[r][1].setText(juce::String(guitarLaneCoords[r].normWidth1, 3), juce::dontSendNotification);
    }

    // Drum notes
    for (int c = 0; c < COL_NOTE_COLS; c++)
        dcolNoteHdrLabels[c].setText(colNoteColNames[c], juce::dontSendNotification);
    for (int r = 0; r < DRUM_LANES; r++)
    {
        const auto& ca = drumColAdjust[r];
        const float vals[COL_NOTE_COLS] = {ca.z, ca.sNear, ca.sFar, ca.w, ca.h};
        for (int c = 0; c < COL_NOTE_COLS; c++)
            dcolNoteParams[r][c].setText(juce::String(vals[c], noteDec[c]), juce::dontSendNotification);
    }

    // Drum lanes
    for (int c = 0; c < COL_LANE_COLS; c++)
        dcolLaneHdrLabels[c].setText(colLaneColNames[c], juce::dontSendNotification);
    for (int r = 0; r < DRUM_LANES; r++)
    {
        dcolLaneParams[r][0].setText(juce::String(drumLaneCoords[r].normX1, 3), juce::dontSendNotification);
        dcolLaneParams[r][1].setText(juce::String(drumLaneCoords[r].normWidth1, 3), juce::dontSendNotification);
    }
}

void DebugTuningPanel::fireLaneChanged()
{
    if (onLaneCoordsChanged) onLaneCoordsChanged();
}

void DebugTuningPanel::fireChanged()
{
    if (onTuningChanged) onTuningChanged();
}

float* DebugTuningPanel::getAdjustPtr(int r, int c)
{
    // Overlay rows (4-8) map to OVERLAY_* enums — columns are X/Y/W/H/S in order.
    if (r >= 4 && r <= 8)
    {
        static constexpr int overlayIdx[5] = {
            PositionConstants::OVERLAY_GUITAR_TAP,
            PositionConstants::OVERLAY_DRUM_NOTE_GHOST,
            PositionConstants::OVERLAY_DRUM_NOTE_ACCENT,
            PositionConstants::OVERLAY_DRUM_CYM_GHOST,
            PositionConstants::OVERLAY_DRUM_CYM_ACCENT
        };
        auto& ov = overlayAdjusts[overlayIdx[r - 4]];
        switch (c) {
        case 0: return &ov.offsetX;
        case 1: return &ov.offsetY;
        case 2: return &ov.scaleX;
        case 3: return &ov.scaleY;
        case 4: return &ov.scale;
        }
        return nullptr;
    }
    auto& gemS = panelIsDrums ? drumGemScale : guitarGemScale;
    auto& barS = panelIsDrums ? drumBarScale : guitarBarScale;
    switch (r)
    {
    case 0: // Note — W/H from baseScale.gem (per-instrument), S from gemTypeScales.normal
        if (c == 2) return &gemS.width;
        if (c == 3) return &gemS.height;
        if (c == 4) return &gemTypeScales.normal;
        break;
    case 1: // Cymbal
        if (c == 4) return &gemTypeScales.cymbal; break;
    case 2: // HOPO
        if (c == 4) return &gemTypeScales.hopo; break;
    case 3: // Bar (kicks + opens) — W/H from baseScale.bar (per-instrument)
        if (c == 2) return &barS.width;
        if (c == 3) return &barS.height;
        break;
    case 9: // SP Gem
        if (c == 4) return &gemTypeScales.spGem; break;
    case 10: // SP Bar
        if (c == 4) return &gemTypeScales.spBar; break;
    }
    return nullptr;
}

void DebugTuningPanel::refreshAdjustLabels()
{
    for (int r = 0; r < ADJUST_ROWS; r++)
        for (int c = 0; c < ADJUST_COLS; c++)
        {
            float* p = getAdjustPtr(r, c);
            if (p != nullptr)
                adjustParams[r][c].setText(juce::String(*p, 2), juce::dontSendNotification);
        }
}

void DebugTuningPanel::setAssetManager(AssetManager& am)
{
    assetManager = &am;
    // Row order: Note, Cymbal, HOPO, Bar, Gtr Tap, Drm Gho, Drm Acc, Cym Gho, Cym Acc, SP Gem, SP Bar
    juce::Image* icons[ADJUST_ROWS] = {
        am.getNoteBlueImage(),            // Note (pick any note color — blue stands out)
        am.getCymYellowImage(),           // Cymbal
        am.getHopoBlueImage(),            // HOPO
        am.getBarKickImage(),             // Bar (kick graphic represents bar notes)
        am.getOverlayNoteTapImage(),      // Gtr Tap
        am.getOverlayNoteGhostImage(),    // Drm Ghost
        am.getOverlayNoteAccentImage(),   // Drm Accent
        am.getOverlayCymGhostImage(),     // Cym Ghost
        am.getOverlayCymAccentImage(),    // Cym Accent
        am.getNoteWhiteImage(),           // SP Gem (white = star power)
        am.getBarWhiteImage(),            // SP Bar
    };
    for (int r = 0; r < ADJUST_ROWS; r++)
    {
        adjustRowIcons[r].icon = icons[r];
        adjustRowIcons[r].repaint();
    }
}

void DebugTuningPanel::refreshLaneShapeLabels()
{
    for (int c = 0; c < LANE_SHAPE_COLS; c++)
        laneShapeColHdrLabels[c].setText(laneShapeColNames[c], juce::dontSendNotification);
    const float vals[LANE_SHAPE_ROWS][LANE_SHAPE_COLS] = {
        {laneShape.startOffset, laneShape.innerStartArc, laneShape.outerStartArc},
        {laneShape.endOffset, laneShape.innerEndArc, laneShape.outerEndArc}
    };
    for (int r = 0; r < LANE_SHAPE_ROWS; r++)
        for (int c = 0; c < LANE_SHAPE_COLS; c++)
            laneShapeParams[r][c].setText(juce::String(vals[r][c], 3), juce::dontSendNotification);
}

void DebugTuningPanel::refreshOverlayLabels()
{
    for (int r = 0; r < NUM_OVERLAY_TYPES; r++)
    {
        const float vals[OVERLAY_PARAMS] = {
            overlayAdjusts[r].offsetX, overlayAdjusts[r].offsetY,
            overlayAdjusts[r].scaleX, overlayAdjusts[r].scaleY, overlayAdjusts[r].scale
        };
        for (int c = 0; c < OVERLAY_PARAMS; c++)
            overlayParamLabels[r][c].setText(juce::String(vals[c], 2), juce::dontSendNotification);
    }
}

void DebugTuningPanel::refreshLabels()
{
    // Perspective labels
    refreshTunableLabels(perspLabels, perspTunables, PERSP_COUNT);

    // Curvature labels
    refreshTunableLabels(curvatureLabels, curvatureTunables, CURVATURE_COUNT);

    // Base Scale table — reflects the active instrument's scales
    for (int r = 0; r < BASE_SCALE_ROWS; r++)
    {
        const auto& s = (r == 0)
            ? (panelIsDrums ? drumGemScale : guitarGemScale)
            : (panelIsDrums ? drumBarScale : guitarBarScale);
        baseScaleParams[r][0].setText(juce::String(s.width, 2), juce::dontSendNotification);
        baseScaleParams[r][1].setText(juce::String(s.height, 2), juce::dontSendNotification);
        baseScaleColHdrLabels[0].setText(baseScaleColNames[0], juce::dontSendNotification);
        baseScaleColHdrLabels[1].setText(baseScaleColNames[1], juce::dontSendNotification);
    }

    // Gem type scales
    refreshTunableLabels(gemTypeLabels, gemTypeTunables, GEM_TYPE_COUNT);

    // Hit Scale table
    for (int r = 0; r < HIT_SCALE_ROWS; r++)
    {
        const auto& s = (r == 0) ? hitGemScale : hitBarScale;
        hitScaleParams[r][0].setText(juce::String(s.scale, 2), juce::dontSendNotification);
        hitScaleParams[r][1].setText(juce::String(s.width, 2), juce::dontSendNotification);
        hitScaleParams[r][2].setText(juce::String(s.height, 2), juce::dontSendNotification);
    }
    for (int c = 0; c < HIT_SCALE_COLS; c++)
        hitScaleColHdrLabels[c].setText(hitScaleColNames[c], juce::dontSendNotification);

    // Hit type scales
    refreshTunableLabels(hitTypeLabels, hitTypeTunables, HIT_TYPE_FLOAT_COUNT);

    {
        static constexpr const char* boolNames[HIT_TYPE_BOOL_COUNT] = {"SP White: ", "Tap Purple:"};
        const bool boolVals[HIT_TYPE_BOOL_COUNT] = {hitTypeConfig.spWhiteFlare, hitTypeConfig.tapPurpleFlare};
        for (int i = 0; i < HIT_TYPE_BOOL_COUNT; i++)
            hitTypeBoolLabels[i].setText(juce::String(boolNames[i]) + (boolVals[i] ? "ON" : "OFF"), juce::dontSendNotification);
    }

    // Z Offsets table
    for (int c = 0; c < Z_COLS; c++)
        zColHdrLabels[c].setText(zColNames[c], juce::dontSendNotification);
    for (int r = 0; r < Z_ROWS; r++)
    {
        const auto& g = guitarOffsets;
        const auto& d = drumOffsets;
        const float gVals[] = {g.gridZ, g.gemZ, g.cymZ, g.barZ, g.hitGemZ, g.hitBarZ};
        const float dVals[] = {d.gridZ, d.gemZ, d.cymZ, d.barZ, d.hitGemZ, d.hitBarZ};
        zParams[r][0].setText(juce::String(gVals[r], 1), juce::dontSendNotification);
        zParams[r][1].setText(juce::String(dVals[r], 1), juce::dontSendNotification);
    }

    // Strike Position table
    for (int c = 0; c < STRIKE_COLS; c++)
        strikeColHdrLabels[c].setText(strikeColNames[c], juce::dontSendNotification);
    strikeParams[0][0].setText(juce::String(guitarOffsets.strikePosGem, 3), juce::dontSendNotification);
    strikeParams[0][1].setText(juce::String(drumOffsets.strikePosGem, 3), juce::dontSendNotification);
    strikeParams[1][0].setText(juce::String(guitarOffsets.strikePosBar, 3), juce::dontSendNotification);
    strikeParams[1][1].setText(juce::String(drumOffsets.strikePosBar, 3), juce::dontSendNotification);

    refreshLaneShapeLabels();
    refreshOverlayLabels();
    refreshAdjustLabels();
}

void DebugTuningPanel::setupTableHeader(juce::Label& label)
{
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colours::grey);
    label.setFont(juce::Font(13.0f));
}

void DebugTuningPanel::setupSectionHeader(SectionHeader& header, const juce::String& text)
{
    header.setTitle(text.toUpperCase());
    header.setJustificationType(juce::Justification::centredLeft);
    header.setColour(juce::Label::textColourId, juce::Colour(0xFF4FC3F7));
    header.setFont(juce::Font(14.0f).boldened());
    header.setInterceptsMouseClicks(true, true);
    header.onToggle = [this]() {
        if (tuningButton.isPanelVisible())
            tuningButton.relayoutPanel();
    };
}

void DebugTuningPanel::setupScrollLabel(ScrollableLabel& label)
{
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    label.setFont(juce::Font(13.0f));
    label.setInterceptsMouseClicks(true, true);
}

int DebugTuningPanel::layoutTable(int y, int x, int w, int rowHeight, int gap,
                                   juce::Label* colHdrs, int numCols,
                                   juce::Label* rowNames, ScrollableLabel* params,
                                   int numRows, int paramStride, int nameW)
{
    const int cellW = (w - nameW) / numCols;

    // Column headers
    for (int c = 0; c < numCols; c++)
    {
        colHdrs[c].setVisible(true);
        colHdrs[c].setBounds(x + nameW + c * cellW, y, cellW, rowHeight);
    }
    y += rowHeight + gap;

    // Rows
    for (int r = 0; r < numRows; r++)
    {
        rowNames[r].setVisible(true);
        rowNames[r].setBounds(x, y, nameW, rowHeight);
        for (int c = 0; c < numCols; c++)
        {
            params[r * paramStride + c].setVisible(true);
            params[r * paramStride + c].setBounds(x + nameW + c * cellW, y, cellW, rowHeight);
        }
        y += rowHeight + gap;
    }
    return y;
}

void DebugTuningPanel::layoutPanel(juce::Component* panel)
{
    const int margin = 12;
    const int rowHeight = 30;
    const int gap = 6;
    const int headerGap = 14;
    int y = margin;
    int w = panel->getWidth() - margin * 2;

    auto layoutRow = [&](juce::Component& comp, bool visible) {
        comp.setVisible(visible);
        if (visible)
        {
            comp.setBounds(margin, y, w, rowHeight);
            y += rowHeight + gap;
        }
    };

    auto hideTable = [](juce::Label* colHdrs, int numCols, juce::Label* rowNames, ScrollableLabel* params, int numRows, int stride) {
        for (int c = 0; c < numCols; c++) colHdrs[c].setVisible(false);
        for (int r = 0; r < numRows; r++) {
            rowNames[r].setVisible(false);
            for (int c = 0; c < numCols; c++) params[r * stride + c].setVisible(false);
        }
    };

    clickZonesToggle.setVisible(true);
    clickZonesToggle.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;

    // --- Z Offsets table (most-used — surface at the top) ---
    zOffsetsHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    if (zOffsetsHeader.expanded)
    {
        y = layoutTable(y, margin, w, rowHeight, gap,
                        zColHdrLabels, Z_COLS,
                        zRowLabels, &zParams[0][0],
                        Z_ROWS, Z_COLS, 34);
    }
    else
    {
        hideTable(zColHdrLabels, Z_COLS, zRowLabels, &zParams[0][0], Z_ROWS, Z_COLS);
    }
    y += headerGap;

    // --- Curvature (guitar / drums) ---
    curvatureHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    layoutTunableRows(curvatureLabels, CURVATURE_COUNT, curvatureHeader.expanded, margin, w, rowHeight, gap, y);
    y += headerGap;

    // --- Lane Width sliders (always visible — quick tune) ---
    layoutRow(laneWidthLabel, true);
    layoutRow(laneOpenWidthLabel, true);
    y += headerGap;

    // --- Unified Adjust: position + scale for every tunable element type ---
    // Inlined (not layoutTable) because row names have a sibling icon component
    // that needs its own position slot in the layout.
    adjustHeader.setVisible(true);
    adjustHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    if (adjustHeader.expanded)
    {
        const int iconW = 22;
        const int nameTextW = 44;
        const int nameW = iconW + nameTextW;
        const int cellW = (w - nameW) / ADJUST_COLS;

        for (int c = 0; c < ADJUST_COLS; c++)
        {
            adjustColHdrLabels[c].setVisible(true);
            adjustColHdrLabels[c].setBounds(margin + nameW + c * cellW, y, cellW, rowHeight);
        }
        y += rowHeight + gap;

        for (int r = 0; r < ADJUST_ROWS; r++)
        {
            adjustRowIcons[r].setVisible(true);
            adjustRowIcons[r].setBounds(margin, y, iconW, rowHeight);
            adjustRowNameLabels[r].setVisible(true);
            adjustRowNameLabels[r].setBounds(margin + iconW, y, nameTextW, rowHeight);
            for (int c = 0; c < ADJUST_COLS; c++)
            {
                adjustParams[r][c].setVisible(true);
                adjustParams[r][c].setBounds(margin + nameW + c * cellW, y, cellW, rowHeight);
            }
            y += rowHeight + gap;
        }
    }
    else
    {
        for (int c = 0; c < ADJUST_COLS; c++) adjustColHdrLabels[c].setVisible(false);
        for (int r = 0; r < ADJUST_ROWS; r++)
        {
            adjustRowIcons[r].setVisible(false);
            adjustRowNameLabels[r].setVisible(false);
            for (int c = 0; c < ADJUST_COLS; c++) adjustParams[r][c].setVisible(false);
        }
    }
    y += headerGap;

    // Legacy sections (data still wired, UI hidden — covered by unified Adjust above)
    overlayAdjustHeader.setVisible(false);
    hideTable(overlayColHeaderLabels, OVERLAY_PARAMS, overlayRowNameLabels, &overlayParamLabels[0][0], NUM_OVERLAY_TYPES, OVERLAY_PARAMS);
    baseScaleHeader.setVisible(false);
    hideTable(baseScaleColHdrLabels, BASE_SCALE_COLS, baseScaleRowLabels, &baseScaleParams[0][0], BASE_SCALE_ROWS, BASE_SCALE_COLS);

    // --- Bemani section (visible — these knobs are the only way to tune
    //     bemani mode; the unified Adjust table only covers perspective). ---
    bemaniHeader.setVisible(true);
    bemaniHeader.setBounds(margin, y, w, rowHeight);
    y += rowHeight + gap;
    if (bemaniHeader.expanded)
    {
        // Group bemani tunables under their subheaders (Position / Sustains / Visual)
        const char* groupNames[3] = {"Position", "Sustains", "Visual"};
        for (int g = 0; g < 3; g++)
        {
            bemaniGroupHeaders[g].setVisible(true);
            bemaniGroupHeaders[g].setBounds(margin, y, w, rowHeight);
            y += rowHeight + gap;
            for (int i = 0; i < BEMANI_TUNABLE_COUNT; i++)
            {
                if (juce::String(bemaniTunables[i].group) != groupNames[g]) continue;
                bemaniLabels[i].setVisible(true);
                bemaniLabels[i].setBounds(margin, y, w, rowHeight);
                y += rowHeight + gap;
            }
        }
    }
    else
    {
        for (int g = 0; g < 3; g++) bemaniGroupHeaders[g].setVisible(false);
        for (int i = 0; i < BEMANI_TUNABLE_COUNT; i++) bemaniLabels[i].setVisible(false);
    }
    y += headerGap;

    // --- Everything below is "advanced" — hidden by default to declutter the
    //     panel. The headers + their content are still wired and tunable; they
    //     just don't render here. To re-expose any section, add a layoutRow /
    //     setBounds call for its header above. Pre-existing data and callbacks
    //     are intact, so nothing breaks if they come back.
    perspectiveHeader.setVisible(false);
    layoutTunableRows(perspLabels, PERSP_COUNT, false, margin, w, rowHeight, gap, y);

    trackHeader.setVisible(false);
    hideTable(layerColHdrLabels, LAYER_COLS, layerRowLabels, &layerParams[0][0], NUM_DISPLAY_LAYERS, LAYER_COLS);
    layoutTunableRows(trackSliderLabels, TRACK_SLIDER_COUNT, false, margin, w, rowHeight, gap, y);
    polyShadeToggle.setVisible(false);
    debugColourToggle.setVisible(false);
    stretchToggle.setVisible(false);
    bemaniToggle.setVisible(false);

    layoutTunableRows(gemTypeLabels, GEM_TYPE_COUNT, false, margin, w, rowHeight, gap, y);

    hitScaleHeader.setVisible(false);
    hideTable(hitScaleColHdrLabels, HIT_SCALE_COLS, hitScaleRowLabels, &hitScaleParams[0][0], HIT_SCALE_ROWS, HIT_SCALE_COLS);
    layoutTunableRows(hitTypeLabels, HIT_TYPE_FLOAT_COUNT, false, margin, w, rowHeight, gap, y);
    for (int i = 0; i < HIT_TYPE_BOOL_COUNT; i++) hitTypeBoolLabels[i].setVisible(false);

    strikeHeader.setVisible(false);
    hideTable(strikeColHdrLabels, STRIKE_COLS, strikeRowLabels, &strikeParams[0][0], STRIKE_ROWS, STRIKE_COLS);

    guitarColsHeader.setVisible(false);
    gcolSubNoteLabel.setVisible(false);
    gcolSubLaneLabel.setVisible(false);
    hideTable(gcolNoteHdrLabels, COL_NOTE_COLS, gcolNoteRowLabels, &gcolNoteParams[0][0], GUITAR_LANES, COL_NOTE_COLS);
    hideTable(gcolLaneHdrLabels, COL_LANE_COLS, gcolLaneRowLabels, &gcolLaneParams[0][0], GUITAR_LANES, COL_LANE_COLS);

    drumColsHeader.setVisible(false);
    dcolSubNoteLabel.setVisible(false);
    dcolSubLaneLabel.setVisible(false);
    hideTable(dcolNoteHdrLabels, COL_NOTE_COLS, dcolNoteRowLabels, &dcolNoteParams[0][0], DRUM_LANES, COL_NOTE_COLS);
    hideTable(dcolLaneHdrLabels, COL_LANE_COLS, dcolLaneRowLabels, &dcolLaneParams[0][0], DRUM_LANES, COL_LANE_COLS);

    laneShapeHeader.setVisible(false);
    hideTable(laneShapeColHdrLabels, LANE_SHAPE_COLS, laneShapeRowLabels, &laneShapeParams[0][0], LANE_SHAPE_ROWS, LANE_SHAPE_COLS);

    panel->setSize(panel->getWidth(), y + margin);
}

#endif
