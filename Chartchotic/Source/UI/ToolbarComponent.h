#pragma once

#include <array>
#include <set>
#include <JuceHeader.h>
#include "ChartchoticLogo.h"
#include "Controls/CircleIconSelector.h"
#include "Controls/PillToggle.h"
#include "Controls/CheckboxToggle.h"
#include "Controls/InfoTooltip.h"
#include "Controls/ValueStepper.h"
#include "Controls/SegmentedButtons.h"
#include "Controls/PanelSectionHeader.h"
#include "Controls/PopupMenuButton.h"
#include "Controls/WriteSubToolbar.h"
#include "MenuGroup.h"
#include "../Utils/ChartTypes.h"
#include "../Visual/Utils/DrawingConstants.h"
#include "ControlConstants.h"

class InteractionController;
#ifdef DEBUG
#include "../DebugTools/DebugToolbarPanel.h"
#include "../DebugTools/DebugTuningPanel.h"
#endif

class ToolbarComponent : public juce::Component
{
public:
    static constexpr float toolbarRatio = 0.06f;    // fraction of editor width (single strip)
    static constexpr int maxToolbarHeight = 100;    // cap toolbar height (pixels)
    static constexpr int referenceHeight = 36;     // design reference for the strip portion
    static constexpr float stripFraction = 1.0f;   // strip is the full toolbar height
    static constexpr float logoFontRatio = 0.90f;  // logo font size as fraction of strip height

    // Sub-toolbar reference height — laid out below the main strip when write
    // mode is active. Scales with the strip the same way other elements do.
    static constexpr float subToolbarHeightRatio = 0.62f; // fraction of the strip height

    // Total height the toolbar reports to its parent. Equals baseStripHeight
    // when write mode is off; baseStripHeight + sub-row when on. PluginEditor
    // calls this in resized() to compute the toolbar bounds and reflow the
    // highway below.
    int getReportedHeight(int baseStripHeight) const;

    // Strip-only height (top row). When the sub-toolbar is visible, this is
    // less than getHeight(); when off, it equals getHeight().
    int getStripHeight() const;

    ToolbarComponent(juce::ValueTree& state, InteractionController& interactionController);
    ~ToolbarComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void loadState();
    void updateVisibility();
    void setReaperMode(bool isReaper);
    bool isReaperModeActive() const { return reaperMode; }
    void setMultiInstrumentMode(bool multi) { multiInstrumentMode = multi; }
    void setPanelTopMargin(int m) { chartButton.setPanelTopMargin(m); settingsButton.setPanelTopMargin(m); }
    void setPanelBottomMargin(int m) { chartButton.setPanelBottomMargin(m); settingsButton.setPanelBottomMargin(m); }

    // Reset instrument selector to simple Guitar/Drums (for discovery-off mode)
    void resetToManualMode();

    // Multi-select instrument/difficulty (Global mode)
    void setDiscoveredParts(const std::vector<Part>& parts);
    void setEnabledParts(const std::set<Part>& parts);
    void setEnabledDifficulties(const std::set<SkillLevel>& diffs);
    void enableMultiDifficultyMode(bool enabled);

    //==============================================================================
    // Callbacks — the editor wires these

    std::function<void(Part part, bool modifierHeld)> onInstrumentClicked;
    std::function<void()> onAllInstrumentsClicked;
    std::function<void(SkillLevel skill, bool modifierHeld)> onDifficultyClicked;
    std::function<void()> onAllDifficultiesClicked;
    std::function<void(int skillId)> onSkillChanged;
    std::function<void(int partId)> onPartChanged;
    std::function<void(int drumTypeId)> onDrumTypeChanged;
    std::function<void(bool enabled)> onAutoHopoChanged;
    std::function<void(int thresholdIndex)> onHopoThresholdChanged;
    std::function<void(bool)> onGemsChanged;
    std::function<void(bool)> onBarsChanged;
    std::function<void(bool)> onSustainsChanged;
    std::function<void(bool)> onLanesChanged;
    std::function<void(bool)> onGridlinesChanged;
    std::function<void(bool)> onHitIndicatorsChanged;
    std::function<void(bool)> onTrackChanged;
    std::function<void(bool)> onLaneSeparatorsChanged;
    std::function<void(bool)> onStrikelineChanged;
    std::function<void(bool)> onHighwayChanged;
    std::function<void(bool)> onStarPowerChanged;
    std::function<void(bool)> onKick2xChanged;
    std::function<void(bool)> onDiscoFlipChanged;
    std::function<void(bool)> onDynamicsChanged;
    std::function<void(int noteSpeed)> onNoteSpeedChanged;
    std::function<void(int framerateId)> onFramerateChanged;
    std::function<void(int latencyId)> onLatencyChanged;
    std::function<void(int offsetMs)> onLatencyOffsetChanged;
    std::function<void(const juce::String& bgName)> onBackgroundChanged;
    std::function<void(const juce::String& textureName)> onHighwayTextureChanged;
    std::function<void(float scale)> onTextureScaleChanged;
    std::function<void(float opacity)> onTextureOpacityChanged;
    std::function<void(float scale)> onGemScaleChanged;
    std::function<void(float scale)> onBarScaleChanged;
    std::function<void(float length)> onHighwayLengthChanged;
    std::function<void(bool)> onStretchChanged;
    std::function<void(bool)> onBemaniModeChanged;
    std::function<void(bool)> onShowFpsChanged;
    std::function<void(bool)> onShowBackgroundChanged;
    std::function<void(bool)> onTrackDiscoveryChanged;
    std::function<void()> onOpenBackgroundFolder;
    std::function<void()> onOpenTextureFolder;

    void setHighwayTextureList(const juce::StringArray& names)
    {
        highwayTextureNames = names;
        if (names.isEmpty())
        {
            highwayTextureIndex = 0;
            highwayTextureStepper.setDisplayValue("n/a");
            highwayTextureStepper.setValueClickable(true);
        }
    }

    void setBackgroundList(const juce::StringArray& names)
    {
        backgroundNames = names;
        if (names.isEmpty())
        {
            backgroundIndex = 0;
            backgroundStepper.setDisplayValue("n/a");
            backgroundStepper.setValueClickable(true);
        }
    }

    void setLatencyOffsetRange(int minMs, int maxMs);

    void setHighwayLengthPct(int pct)
    {
        highwayLengthPct = juce::jlimit(HWY_LENGTH_MIN_PCT, HWY_LENGTH_MAX_PCT, pct);
        highwayLengthStepper.setDisplayValue(highwayLengthPct);
    }

    // Expose for scroll-wheel handling in editor
    ValueStepper& getNoteSpeedStepper() { return noteSpeedStepper; }
    ChartchoticLogo& getLogo() { return logo; }

    // Re-read write-mode state from WriteController and refresh the
    // sub-toolbar. Wired to WriteController::onStateChanged in PluginEditor.
    // Returns true if the toolbar's reported height changed (i.e. write-mode
    // visibility flipped) so the caller can trigger a parent re-layout to
    // reclaim/yield highway space.
    bool refreshFromWriteController();

    // Back-compat alias — equivalent to refreshFromWriteController() but
    // discards the height-changed return.
    void repaintModePill() { refreshFromWriteController(); }

private:
    juce::ValueTree& state;
    InteractionController& interactionController;
    bool reaperMode = false;
    bool multiInstrumentMode = false;

    //==============================================================================
    // Top bar — always visible

    ChartchoticLogo logo;
    CircleIconSelector instrumentSelector;
    CircleIconSelector difficultySelector;
    ValueStepper noteSpeedStepper{"Speed"};

    // Second toolbar row — only visible while write mode is active.
    WriteSubToolbar writeSubToolbar { interactionController };

    // Multi-select instrument state (Global mode with 2+ parts)
    std::vector<Part> discoveredParts;
    std::set<Part> enabledParts;
    bool showMultiInstrument = false;
    bool showMultiDifficulty = false;

    //==============================================================================
    // View panel — Modifiers (contextual per instrument)

    PanelSectionHeader modifiersHeader{"Modifiers"};
    PillToggle starPowerToggle{"Star Power"};
    PillToggle autoHopoToggle{"Auto HOPO"};
    PillToggle dynamicsToggle{"Dynamics"};
    PillToggle kick2xToggle{"Kick 2x"};
    PillToggle cymbalsToggle{"Cymbals"};
    PillToggle discoFlipToggle{"Disco Flip"};
    ValueStepper hopoThresholdStepper{"Threshold"};
    int hopoThresholdIndex = HOPO_THRESHOLD_DEFAULT;

    // Modifier visibility: which instrument type each toggle requires
    enum class ModScope { ALL, GUITAR, DRUMS };
    struct ModToggle { PillToggle* pill; ModScope scope; };
    // Order here = display order in the chart panel (flows left-to-right, 2 per row)
    std::array<ModToggle, 6> modToggles {{
        { &starPowerToggle,  ModScope::ALL },
        { &cymbalsToggle,    ModScope::DRUMS },
        { &dynamicsToggle,   ModScope::DRUMS },
        { &kick2xToggle,     ModScope::DRUMS },
        { &discoFlipToggle,  ModScope::DRUMS },
        { &autoHopoToggle,   ModScope::GUITAR },
    }};

    // View panel — Chart elements
    PanelSectionHeader chartHeader{"Chart"};
    PillToggle gemsToggle{"Gems"};
    PillToggle barsToggle{"Bars"};
    PillToggle sustainsToggle{"Sustains"};
    PillToggle lanesToggle{"Lanes"};
    PillToggle gridlinesToggle{"Gridlines"};
    PillToggle hitIndicatorsToggle{"Hits"};

    // View panel — Scene elements
    PanelSectionHeader sceneHeader{"Scene"};
    PillToggle trackToggle{"Track"};
    PillToggle laneSeparatorsToggle{"Rails"};
    PillToggle strikelineToggle{"Strike"};
    PillToggle highwayToggle{"Hwy"};

    //==============================================================================
    // Settings panel (gear icon) — Display, Highway, Sync

    PanelSectionHeader displayHeader{"Display"};
    PanelSectionHeader highwayHeader{"Highway"};
    ValueStepper highwayLengthStepper{"Length", "%"};
    ValueStepper backgroundStepper{"Background"};
    ValueStepper highwayTextureStepper{"Texture"};
    juce::Label textureScaleLabel;
    juce::Label textureOpacityLabel;
    ValueStepper textureScaleStepper{"Scale", "%"};
    ValueStepper textureOpacityStepper{"Opacity", "%"};
    ValueStepper gemScaleStepper{"Gem Size", "%"};
    ValueStepper barScaleStepper{"Bar Size", "%"};


    PanelSectionHeader syncHeader{"Sync"};
    ValueStepper syncOffsetStepper{"Calibration", " ms"};
    int syncOffsetMs = CALIBRATION_DEFAULT;
    int syncOffsetMin = CALIBRATION_MIN_MS;
    int syncOffsetMax = CALIBRATION_MAX_MS;
    ValueStepper latencyStepper{"Latency"};
    int latencyIndex = LATENCY_DEFAULT - 1; // state is 1-based
    SegmentedButtons framerateButtons;
    CheckboxToggle stretchToggle{"Stretch"};
    CheckboxToggle bemaniModeToggle{juce::CharPointer_UTF8("\xe3\x83\x93\xe3\x83\xbc\xe3\x83\x9e\xe3\x83\x8b"), "Bemani Mode"}; // ビーマニ
    CheckboxToggle showFpsToggle{"Show FPS"};
    CheckboxToggle showBackgroundToggle{"Background"};
    CheckboxToggle trackDiscoveryToggle{"Track Discovery"};
    InfoTooltip trackDiscoveryTooltip;
    InfoTooltip calibrationTooltip;
    InfoTooltip latencyTooltip;
    InfoTooltip discoFlipTooltip;
    InfoTooltip hopoThresholdTooltip;

    //==============================================================================
    // Value data
    int noteSpeed = NOTE_SPEED_DEFAULT;
    int backgroundIndex = 0;
    int highwayTextureIndex = 0;
    int textureOpacityPct = TEX_OPACITY_DEFAULT;
    int gemScalePct = GEM_SCALE_DEFAULT_PCT;
    int barScalePct = BAR_SCALE_DEFAULT_PCT;
    int highwayLengthPct = HWY_LENGTH_DEFAULT_PCT;

    juce::StringArray backgroundNames;
    juce::StringArray highwayTextureNames;

    int texScalePct = TEX_SCALE_DEFAULT_PCT;

    //==============================================================================
    // Menu groups (must outlive their members)

    MenuGroup circleMenuGroup;
    MenuGroup squareMenuGroup;

    //==============================================================================
    // Popup buttons

    PopupMenuButton chartButton{"View"};
    PopupMenuButton settingsButton{juce::CharPointer_UTF8("\xe2\x9a\x99")}; // ⚙

    //==============================================================================

#ifdef DEBUG
    DebugToolbarPanel debugPanel{state};
    DebugTuningPanel tuningPanel{state};
public:
    DebugToolbarPanel& getDebugPanel() { return debugPanel; }
    DebugTuningPanel& getTuningPanel() { return tuningPanel; }
    void setDebugPlay(bool playing);
private:
#endif

    void initTopBar();
    void initManualInstrumentSelector();
    void initChartPanel();
    void initSettingsPanel();
    void layoutChartPanel(juce::Component* panel);
    void layoutSettingsPanel(juce::Component* panel);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToolbarComponent)
};
