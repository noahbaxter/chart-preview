#pragma once

#ifdef DEBUG

#include <JuceHeader.h>
#include "../UI/Controls/PopupMenuButton.h"
#include "../UI/SectionHeader.h"
#include "../Utils/ChartTypes.h"
#include "../Visual/Utils/PositionConstants.h"
#include "../Visual/Utils/PositionMath.h"
#include "../Visual/Utils/DrawingConstants.h"
#include "../Visual/Renderers/TrackRenderer.h"

class SceneRenderer;
class AssetManager;

// Universal descriptor for a single debug tuning slider
struct DebugTunable
{
    const char* name;
    float* value;          // direct pointer to the backing float
    float min, max, step;
    int decimals;
    bool featured = false; // highlighted in the tuning panel for quick access
};

class DebugTuningPanel
{
public:
    DebugTuningPanel(juce::ValueTree& state);
    ~DebugTuningPanel();

    PopupMenuButton& getButton() { return tuningButton; }

    // Apply all tuning values to a SceneRenderer
    void applyTo(SceneRenderer& sr) const;

    // Initialize layer/tiling defaults from current TrackRenderer state
    void initDefaults(const TrackRenderer& trackRenderer);

    // Switch layer states between guitar/drums
    void setDrums(bool isDrums);

    // Hook up AssetManager so row-name labels show the element's glyph.
    void setAssetManager(AssetManager& am);

    // Callbacks
    std::function<void()> onTuningChanged;
    std::function<void(int, float, float, float)> onLayerChanged;
    std::function<void(float)> onTileStepChanged;
    std::function<void(float)> onTileScaleStepChanged;
    std::function<void(float)> onTextureScaleChanged;
    std::function<void(float)> onTextureOpacityChanged;
    std::function<void()> onPolyShadeChanged;
    std::function<void(bool)> onDebugColourChanged;
    std::function<void(bool)> onClickZonesChanged;
    std::function<void(bool)> onStretchChanged;
    std::function<void(bool)> onBemaniToggled;
    std::function<void(float, float)> onHwyScaleChanged;
    std::function<void()> onBemaniTuningChanged;
    std::function<void(float gap, float nudge)> onLogoPadChanged;

    // Current tuning values (read by applyTo)
    float guitarCurvature = PositionConstants::NOTE_CURVATURE;
    float drumCurvature = PositionConstants::NOTE_CURVATURE_DRUMS;

    // Base note/bar scale, per-instrument. Adjust table edits the active one
    // (selected by setDrums()); applyTo() syncs both pairs to SceneRenderer.
    PositionConstants::ElementScale guitarGemScale = PositionConstants::GUITAR_GEM_SCALE;
    PositionConstants::ElementScale drumGemScale   = PositionConstants::DRUM_GEM_SCALE;
    PositionConstants::ElementScale guitarBarScale = PositionConstants::GUITAR_BAR_SCALE;
    PositionConstants::ElementScale drumBarScale   = PositionConstants::DRUM_BAR_SCALE;

    // Hit animation scale (HitScale table: 2 rows x 3 cols)
    PositionConstants::HitScale hitGemScale = PositionConstants::HIT_GEM_SCALE;
    PositionConstants::HitScale hitBarScale = PositionConstants::HIT_BAR_SCALE;

    // Per-hit-type scales + flare config
    PositionConstants::HitTypeConfig hitTypeConfig;

    // Per-gem-type scales
    PositionConstants::GemTypeScales gemTypeScales;

    // Per-instrument offsets (Z + strike positions, table: 5+2 rows x 2 cols)
    PositionConstants::InstrumentOffsets guitarOffsets = PositionConstants::GUITAR_OFFSETS;
    PositionConstants::InstrumentOffsets drumOffsets = PositionConstants::DRUM_OFFSETS;

    // Per-column adjustments (Z offset, scale, W/H)
    PositionConstants::ColumnAdjust guitarColAdjust[6] = {
        PositionConstants::GUITAR_COL_ADJUST[0], PositionConstants::GUITAR_COL_ADJUST[1],
        PositionConstants::GUITAR_COL_ADJUST[2], PositionConstants::GUITAR_COL_ADJUST[3],
        PositionConstants::GUITAR_COL_ADJUST[4], PositionConstants::GUITAR_COL_ADJUST[5]};
    PositionConstants::ColumnAdjust drumColAdjust[5] = {
        PositionConstants::DRUM_COL_ADJUST[0], PositionConstants::DRUM_COL_ADJUST[1],
        PositionConstants::DRUM_COL_ADJUST[2], PositionConstants::DRUM_COL_ADJUST[3],
        PositionConstants::DRUM_COL_ADJUST[4]};

    // Overlay adjustments (mutable, for real-time tuning)
    PositionConstants::OverlayAdjust overlayAdjusts[PositionConstants::NUM_OVERLAY_TYPES];

    // Per-lane coordinates (mutable copies of BezierLaneCoords)
    static constexpr int GUITAR_LANES = 6;
    static constexpr int DRUM_LANES = 5;
    PositionConstants::NormalizedCoordinates guitarLaneCoords[GUITAR_LANES];
    PositionConstants::NormalizedCoordinates drumLaneCoords[DRUM_LANES];

    std::function<void()> onLaneCoordsChanged;

    // Lane shape config (tuneable)
    PositionConstants::LaneShapeConfig laneShape;

    // Perspective params (tuneable, synced to PositionMath::debugPerspParams)
    std::function<void()> onPerspectiveChanged;

private:
    juce::ValueTree& state;
    PopupMenuButton tuningButton{"T"};

    class ScrollableLabel : public juce::Label
    {
    public:
        // Scroll / drag / arrow-key: adjust by one step per delta tick.
        std::function<void(int delta)> onScroll;
        // Double-click-to-edit commit: absolute value typed by user.
        // Each site that cares about precise entry should wire both onScroll + onSet.
        std::function<void(float absolute)> onSet;

        ScrollableLabel()
        {
            // Double-click to edit; keep typed value on focus loss (don't discard).
            setEditable(false, true, false);
            // Single click focuses the cell so arrow keys nudge the value.
            setWantsKeyboardFocus(true);
            setMouseClickGrabsKeyboardFocus(true);
        }

        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
        {
            if (isBeingEdited()) return;
            int delta = (wheel.deltaY > 0) ? 1 : -1;
            if (onScroll) onScroll(delta);
        }
        void mouseDown(const juce::MouseEvent& e) override
        {
            if (isBeingEdited()) { juce::Label::mouseDown(e); return; }
            lastDragY = e.y;
        }
        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (isBeingEdited()) { juce::Label::mouseDrag(e); return; }
            int diff = lastDragY - e.y;
            int steps = diff / dragPixelsPerStep;
            if (steps != 0 && onScroll)
            {
                for (int i = 0; i < std::abs(steps); i++)
                    onScroll(steps > 0 ? 1 : -1);
                lastDragY -= steps * dragPixelsPerStep;
            }
        }

        // Arrow keys nudge the value when the cell has keyboard focus (set by
        // clicking the cell). Routes through onScroll so the step size matches
        // wheel/drag. Ignored while the text editor is open — the editor
        // consumes arrow keys for cursor navigation.
        bool keyPressed(const juce::KeyPress& key) override
        {
            if (isBeingEdited()) return false;
            const int code = key.getKeyCode();
            if (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey)
            {
                if (onScroll) onScroll(code == juce::KeyPress::upKey ? 1 : -1);
                return true;
            }
            return juce::Label::keyPressed(key);
        }

        void focusGained(juce::Component::FocusChangeType) override { repaint(); }
        void focusLost(juce::Component::FocusChangeType) override   { repaint(); }

        void paint(juce::Graphics& g) override
        {
            juce::Label::paint(g);
            if (hasKeyboardFocus(false) && !isBeingEdited())
            {
                g.setColour(juce::Colour(Theme::coral).withAlpha(0.7f));
                g.drawRect(getLocalBounds(), 1);
            }
        }

    protected:
        // When the editor opens, strip the padded "  name     value" rows down to
        // just the numeric token so the user doesn't have to edit around the label.
        // Grid cells (text is already just "1.23") keep their text as-is.
        void editorShown(juce::TextEditor* ed) override
        {
            juce::Label::editorShown(ed);
            if (ed == nullptr) return;
            auto text = getText().trim();
            auto lastSpace = text.lastIndexOfAnyOf(" \t");
            if (lastSpace > 0)
                ed->setText(text.substring(lastSpace + 1), juce::dontSendNotification);
            ed->selectAll();
        }

        void textWasEdited() override
        {
            juce::Label::textWasEdited();
            if (onSet)
                onSet(getText().trim().getFloatValue());
        }

    private:
        int lastDragY = 0;
        static constexpr int dragPixelsPerStep = 3;
    };

    // --- Helper: init a range of ScrollableLabels from a DebugTunable array ---
    void initTunableSliders(ScrollableLabel* labels, const DebugTunable* tunables,
                            int count, std::function<void()> onChange);
    void addTunableChildren(ScrollableLabel* labels, int count);
    void layoutTunableRows(ScrollableLabel* labels, int count, bool visible,
                           int margin, int w, int rowHeight, int gap, int& y);
    void refreshTunableLabels(ScrollableLabel* labels, const DebugTunable* tunables, int count);

    // --- Perspective section ---
    SectionHeader perspectiveHeader;
    static constexpr int PERSP_COUNT = 7;
    ScrollableLabel perspLabels[PERSP_COUNT];
    DebugTunable perspTunables[PERSP_COUNT];

    // --- Track section (layers table + tiling) ---
    SectionHeader trackHeader;
    static constexpr int NUM_TRACK_LAYERS = 4;  // storage (matches TrackRenderer::NUM_LAYERS)
    static constexpr int NUM_DISPLAY_LAYERS = 3; // UI rows (excludes LANE_LINES)
    static constexpr int LAYER_COLS = 3;
    static constexpr int displayLayerMap[NUM_DISPLAY_LAYERS] = {0, 2, 3}; // SIDEBARS, STRIKELINE, CONNECTORS
    static constexpr const char* layerNames[NUM_DISPLAY_LAYERS] = {"Side", "Strike", "Conn"};
    static constexpr const char* layerColNames[LAYER_COLS] = {"S", "X", "Y"};

    using LayerTransform = TrackRenderer::LayerTransform;
    LayerTransform guitarStates[NUM_TRACK_LAYERS];
    LayerTransform drumStates[NUM_TRACK_LAYERS];
    LayerTransform* layerStates = guitarStates;

    // Tracks the active instrument for getAdjustPtr() routing
    bool panelIsDrums = false;

    juce::Label layerColHdrLabels[LAYER_COLS];
    juce::Label layerRowLabels[NUM_DISPLAY_LAYERS];
    ScrollableLabel layerParams[NUM_DISPLAY_LAYERS][LAYER_COLS];

    // Track section individual sliders (data-driven)
    float tileStepValue = 0.80f;
    float tileScaleStepValue = 0.50f;
    float textureScaleValue = 1.0f;
    float textureOpacityValue = TEXTURE_OPACITY_DEFAULT;
    float hwyScaleGuitarValue = bemaniConfig.hwyScaleGuitar;
    float hwyScaleDrumsValue = bemaniConfig.hwyScaleDrums;
    float logoPadValue = 0.1f;
    float dotNudgeValue = -0.04f;
    float gridlinePosOffset = PositionConstants::GRIDLINE_POS_OFFSET;

    static constexpr int TRACK_SLIDER_COUNT = 9;
    ScrollableLabel trackSliderLabels[TRACK_SLIDER_COUNT];
    DebugTunable trackSliderTunables[TRACK_SLIDER_COUNT];

    // Debug visualization toggles
    juce::ToggleButton polyShadeToggle;
    juce::ToggleButton debugColourToggle;
    juce::ToggleButton clickZonesToggle;
    juce::ToggleButton stretchToggle;
    juce::ToggleButton bemaniToggle;

    // --- Bemani section (data-driven from BemaniConfig.h) ---
    SectionHeader bemaniHeader;
    ScrollableLabel bemaniLabels[BEMANI_TUNABLE_COUNT];
    juce::Label bemaniGroupHeaders[3]; // Position, Sustains, Visual

    void fireLayer(int idx);
    void refreshTrackLabels();

    // --- Curvature section ---
    SectionHeader curvatureHeader;
    static constexpr int CURVATURE_COUNT = 2;
    ScrollableLabel curvatureLabels[CURVATURE_COUNT];
    DebugTunable curvatureTunables[CURVATURE_COUNT];

    // --- Base Scale table (2 rows x 2 cols: W, H) ---
    SectionHeader baseScaleHeader;
    static constexpr int BASE_SCALE_ROWS = 2;
    static constexpr int BASE_SCALE_COLS = 2;
    static constexpr const char* baseScaleRowNames[BASE_SCALE_ROWS] = {"Gem", "Bar"};
    static constexpr const char* baseScaleColNames[BASE_SCALE_COLS] = {"W", "H"};
    juce::Label baseScaleColHdrLabels[BASE_SCALE_COLS];
    juce::Label baseScaleRowLabels[BASE_SCALE_ROWS];
    ScrollableLabel baseScaleParams[BASE_SCALE_ROWS][BASE_SCALE_COLS];

    // --- Gem type scale (data-driven, backed by gemTypeScales struct) ---
    static constexpr int GEM_TYPE_COUNT = 10;
    ScrollableLabel gemTypeLabels[GEM_TYPE_COUNT];
    DebugTunable gemTypeTunables[GEM_TYPE_COUNT];

    // --- Hit Scale table (2 rows x 3 cols: S, W, H) ---
    SectionHeader hitScaleHeader;
    static constexpr int HIT_SCALE_ROWS = 2;
    static constexpr int HIT_SCALE_COLS = 3;
    static constexpr const char* hitScaleRowNames[HIT_SCALE_ROWS] = {"Gem", "Bar"};
    static constexpr const char* hitScaleColNames[HIT_SCALE_COLS] = {"S", "W", "H"};
    juce::Label hitScaleColHdrLabels[HIT_SCALE_COLS];
    juce::Label hitScaleRowLabels[HIT_SCALE_ROWS];
    ScrollableLabel hitScaleParams[HIT_SCALE_ROWS][HIT_SCALE_COLS];

    // --- Hit type scale (data-driven, backed by hitTypeConfig struct) ---
    static constexpr int HIT_TYPE_FLOAT_COUNT = 5;
    static constexpr int HIT_TYPE_BOOL_COUNT = 2;
    ScrollableLabel hitTypeLabels[HIT_TYPE_FLOAT_COUNT];
    DebugTunable hitTypeTunables[HIT_TYPE_FLOAT_COUNT];
    ScrollableLabel hitTypeBoolLabels[HIT_TYPE_BOOL_COUNT];

    // --- Z Offsets table (6 rows x 2 cols: Guitar, Drums) ---
    // Cym is drums-only; for guitar it's effectively dead (cymZ unused).
    SectionHeader zOffsetsHeader;
    static constexpr int Z_ROWS = 6;
    static constexpr int Z_COLS = 2;
    static constexpr const char* zRowNames[Z_ROWS] = {"Grid", "Gem", "Cym", "Bar", "HitN", "HitB"};
    static constexpr const char* zColNames[Z_COLS] = {"Gtr", "Drm"};
    juce::Label zColHdrLabels[Z_COLS];
    juce::Label zRowLabels[Z_ROWS];
    ScrollableLabel zParams[Z_ROWS][Z_COLS];

    // --- Strike Position table (2 rows x 2 cols: Guitar, Drums) ---
    SectionHeader strikeHeader;
    static constexpr int STRIKE_ROWS = 2;
    static constexpr int STRIKE_COLS = 2;
    static constexpr const char* strikeRowNames[STRIKE_ROWS] = {"Gem", "Bar"};
    static constexpr const char* strikeColNames[STRIKE_COLS] = {"Gtr", "Drm"};
    juce::Label strikeColHdrLabels[STRIKE_COLS];
    juce::Label strikeRowLabels[STRIKE_ROWS];
    ScrollableLabel strikeParams[STRIKE_ROWS][STRIKE_COLS];

    // --- Guitar Cols section (notes table + lanes table) ---
    SectionHeader guitarColsHeader;
    static constexpr const char* guitarColNames[GUITAR_LANES] = {"Open", "Grn", "Red", "Yel", "Blu", "Org"};
    static constexpr int COL_NOTE_COLS = 5;   // Z, S1, S2, W, H
    static constexpr int COL_LANE_COLS = 2;   // X, W
    static constexpr const char* colNoteColNames[COL_NOTE_COLS] = {"Z", "S1", "S2", "W", "H"};
    static constexpr const char* colLaneColNames[COL_LANE_COLS] = {"X", "W"};
    juce::Label gcolSubNoteLabel;   // "Notes" sub-header
    juce::Label gcolSubLaneLabel;   // "Lanes" sub-header
    juce::Label gcolNoteHdrLabels[COL_NOTE_COLS];
    juce::Label gcolNoteRowLabels[GUITAR_LANES];
    ScrollableLabel gcolNoteParams[GUITAR_LANES][COL_NOTE_COLS];
    juce::Label gcolLaneHdrLabels[COL_LANE_COLS];
    juce::Label gcolLaneRowLabels[GUITAR_LANES];
    ScrollableLabel gcolLaneParams[GUITAR_LANES][COL_LANE_COLS];

    // --- Drum Cols section (notes table + lanes table) ---
    SectionHeader drumColsHeader;
    static constexpr const char* drumColNames[DRUM_LANES] = {"Kick", "Red", "Yel", "Blu", "Grn"};
    juce::Label dcolSubNoteLabel;   // "Notes" sub-header
    juce::Label dcolSubLaneLabel;   // "Lanes" sub-header
    juce::Label dcolNoteHdrLabels[COL_NOTE_COLS];
    juce::Label dcolNoteRowLabels[DRUM_LANES];
    ScrollableLabel dcolNoteParams[DRUM_LANES][COL_NOTE_COLS];
    juce::Label dcolLaneHdrLabels[COL_LANE_COLS];
    juce::Label dcolLaneRowLabels[DRUM_LANES];
    ScrollableLabel dcolLaneParams[DRUM_LANES][COL_LANE_COLS];

    // --- Lane Width sliders (above Lane Shape) ---
    ScrollableLabel laneWidthLabel;
    ScrollableLabel laneOpenWidthLabel;

    // --- Lane Shape section (2 rows x 3 cols: Offset, Inner, Outer) ---
    SectionHeader laneShapeHeader;
    static constexpr int LANE_SHAPE_ROWS = 2;
    static constexpr int LANE_SHAPE_COLS = 3;
    static constexpr const char* laneShapeRowNames[LANE_SHAPE_ROWS] = {"Start", "End"};
    static constexpr const char* laneShapeColNames[LANE_SHAPE_COLS] = {"Pos", "Arc", "Bow"};
    juce::Label laneShapeColHdrLabels[LANE_SHAPE_COLS];
    juce::Label laneShapeRowLabels[LANE_SHAPE_ROWS];
    ScrollableLabel laneShapeParams[LANE_SHAPE_ROWS][LANE_SHAPE_COLS];
    void refreshLaneShapeLabels();

    // --- Overlay Adjust section (legacy, hidden — covered by unified Adjust below) ---
    SectionHeader overlayAdjustHeader;
    static constexpr int NUM_OVERLAY_TYPES = PositionConstants::NUM_OVERLAY_TYPES;
    static constexpr int OVERLAY_PARAMS = 5;
    static constexpr const char* overlayRowNames[NUM_OVERLAY_TYPES] = {"GTap", "DGho", "DAcc", "CGho", "CAcc"};
    static constexpr const char* overlayColNames[OVERLAY_PARAMS] = {"X", "Y", "W", "H", "S"};
    juce::Label overlayColHeaderLabels[OVERLAY_PARAMS];
    juce::Label overlayRowNameLabels[NUM_OVERLAY_TYPES];
    ScrollableLabel overlayParamLabels[NUM_OVERLAY_TYPES][OVERLAY_PARAMS];
    void refreshOverlayLabels();

    // --- Unified Adjust table: position + scale for every tunable element type.
    //     Rows pull from baseScale (gem/bar), gemTypeScales, and overlayAdjusts.
    //     Cells for data that doesn't exist render as "—" and do nothing.
    SectionHeader adjustHeader;
    static constexpr int ADJUST_ROWS = 11;
    static constexpr int ADJUST_COLS = 5;
    static constexpr const char* adjustRowNames[ADJUST_ROWS] = {
        "Note", "Cymbal", "HOPO", "Bar",
        "Gtr Tap", "Drm Gho", "Drm Acc", "Cym Gho", "Cym Acc",
        "SP Gem", "SP Bar"
    };
    static constexpr const char* adjustColNames[ADJUST_COLS] = {"X", "Y", "W", "H", "S"};

    // Small image component used as a sibling icon beside each row-name label.
    // Kept as a juce::Component (not a Label subclass) so layoutTable's
    // pointer-arithmetic on juce::Label* stays valid.
    class IconImg : public juce::Component
    {
    public:
        juce::Image* icon = nullptr;
        void paint(juce::Graphics& g) override
        {
            if (icon == nullptr || !icon->isValid()) return;
            const int size = juce::jmin(getWidth(), getHeight()) - 2;
            if (size <= 0) return;
            const int x = (getWidth() - size) / 2;
            const int y = (getHeight() - size) / 2;
            g.drawImage(*icon, juce::Rectangle<int>(x, y, size, size).toFloat(),
                        juce::RectanglePlacement::centred);
        }
    };

    juce::Label adjustColHdrLabels[ADJUST_COLS];
    juce::Label adjustRowNameLabels[ADJUST_ROWS];
    IconImg adjustRowIcons[ADJUST_ROWS];
    ScrollableLabel adjustParams[ADJUST_ROWS][ADJUST_COLS];
    float* getAdjustPtr(int row, int col);
    void refreshAdjustLabels();
    AssetManager* assetManager = nullptr;

    void refreshColLabels();
    void fireLaneChanged();

    // Table layout helpers
    void setupTableHeader(juce::Label& label);
    void setupSectionHeader(SectionHeader& header, const juce::String& text);
    void setupScrollLabel(ScrollableLabel& label);
    void refreshLabels();
    void layoutPanel(juce::Component* panel);
    void fireChanged();

    // Generic table layout: positions column headers, row names, and cells
    // Returns Y after the table. nameW = width of row name column.
    int layoutTable(int y, int x, int w, int rowHeight, int gap,
                    juce::Label* colHdrs, int numCols,
                    juce::Label* rowNames, ScrollableLabel* params,
                    int numRows, int paramStride, int nameW);
};

#endif
