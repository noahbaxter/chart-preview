#pragma once

#include <cmath>
#include <vector>
#include <JuceHeader.h>

//==============================================================================
// Authoring types shared between HighwayComponent (event dispatcher),
// WriteController (state owner), and GridlineGenerator (step grid).

struct AuthoringPoint
{
    juce::Point<float> screenPos;
    bool   onHighway = false;        // coupled with laneIndex: true ⇔ laneIndex >= 0
    int    laneIndex = -1;
    double rawProjectQN = 0.0;
    bool   overExistingNote = false;
    bool   hitSustainBody = false;
    double hitNoteStartQN = 0.0;
    int    hitNotePitch = -1;
};

struct AuthoringContext
{
    juce::ModifierKeys mods;
    bool leftButton = false;
    bool rightButton = false;
};

struct SelectedNote
{
    int    trackIdx = -1;
    double startQN  = 0.0;
    int    pitch    = -1;
    int    lane     = -1;
};

struct OverlayState
{
    struct PreviewNote
    {
        int    lane = -1;
        double startQN = 0.0;
        double endQN = 0.0;
        int    pitch = -1;
    };

    // Hover ghost
    bool   ghostVisible = false;
    int    ghostLane = -1;
    double ghostQN = 0.0;
    bool   ghostShowsErase = false;

    // Draw stroke preview
    bool                     drawPreviewVisible = false;
    std::vector<PreviewNote> drawPreviewNotes;

    // Erase sweep preview
    bool                     eraseSweepVisible = false;
    std::vector<PreviewNote> eraseSweepTargets;

    // Selection (edit mode)
    std::vector<SelectedNote> selectedNotes;
    bool selectionBoundsVisible = false;
    struct SelectionBounds {
        int    minLane = 0;
        int    maxLane = 0;
        double minQN  = 0.0;
        double maxQN  = 0.0;
    } selectionBounds;

    // Move drag preview
    bool                     moveDragVisible = false;
    std::vector<PreviewNote> movePreviewNotes;

    // Notes to hide from normal rendering (during move drag + post-commit delay)
    std::vector<SelectedNote> hideNotes;

    // Marquee selection (edit mode) — highway-space coordinates
    bool   marqueeVisible = false;
    int    marqueeLaneStart = 0;
    int    marqueeLaneEnd   = 0;
    double marqueeQNStart   = 0.0;
    double marqueeQNEnd     = 0.0;
};

enum class EventType { Down, Drag, Up, DoubleClick };

enum class MouseButton { None, Left, Right, Middle };

enum class ModifierFlags : int {
    None  = 0,
    Shift = 1 << 0,
    Ctrl  = 1 << 1,
    Alt   = 1 << 2,
};
inline ModifierFlags operator|(ModifierFlags a, ModifierFlags b) {
    return static_cast<ModifierFlags>(static_cast<int>(a) | static_cast<int>(b));
}
inline bool operator&(ModifierFlags a, ModifierFlags b) {
    return (static_cast<int>(a) & static_cast<int>(b)) != 0;
}

enum class WriteCommand {
    None,
    // Draw-mode mouse commands
    BeginSustain,
    UpdateSustain,
    CommitSustain,
    BeginPaint,
    ContinuePaint,
    CommitPaint,
    BeginErase,
    ContinueErase,
    EndErase,
    // Edit-mode mouse commands
    SelectAt,
    BeginMarquee,
    ContinueMarquee,
    CommitMarquee,
    BeginMove,
    ContinueMove,
    CommitMove,
    DoubleClick,
    // Key commands
    ToggleWriteMode,
    ToggleSubMode,
    ToggleSnap,
    CycleTuplet,
    StepDown,
    StepUp,
    DeleteSelection,
    DeselectAll,
};

enum class SubMode { Draw, Edit };

//==============================================================================
// Step grid math — shared between WriteController (click snap) and
// GridlineGenerator (visual grid). One definition, no drift.

inline double stepSpacingQN(int stepDivision, int tuplet)
{
    if (tuplet > 0)
        return 8.0 / (double(stepDivision) * double(tuplet));
    return 4.0 / double(stepDivision);
}

inline double snapToStep(double rawQN, int stepDivision, int tuplet)
{
    const double spacing = stepSpacingQN(stepDivision, tuplet);
    if (spacing <= 0.0)
        return rawQN;

    double snapped = std::round(rawQN / spacing) * spacing;

    const double oneOver128 = 1.0 / 128.0;
    const double quantized  = std::round(snapped / oneOver128) * oneOver128;
    if (std::abs(snapped - quantized) < 1e-6)
        snapped = quantized;
    return snapped;
}
