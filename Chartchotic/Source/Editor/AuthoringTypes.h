#pragma once

#include <cmath>
#include <vector>
#include <JuceHeader.h>
#include "../Utils/ChartTypes.h"

//==============================================================================
// Authoring types shared between HighwayComponent (event dispatcher),
// WriteController (state owner), and GridlineGenerator (step grid).

constexpr double kQNEpsilon   = 0.001;
constexpr double kTimeEpsilon = 0.002;

namespace AuthoringColours
{
    static const juce::Colour selectTint = juce::Colour(180, 220, 255).withAlpha((uint8)140);
    static const juce::Colour eraseTint  = juce::Colour(255, 80, 80).withAlpha((uint8)160);
}

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
    int    trackIdx    = -1;
    double startQN     = 0.0;
    double endQN       = 0.0;
    int    pitch       = -1;
    int    lane        = -1;
    bool   sustainOnly = false;
};

struct MarqueeRect
{
    double startQN = 0.0;
    int    startLane = 0;
    double qnLo = 0.0, qnHi = 0.0;
    int    laneLo = 0, laneHi = 0;

    void begin(double qn, int lane)
    {
        startQN = qn; startLane = lane;
        qnLo = qnHi = qn;
        laneLo = laneHi = lane;
    }

    void update(double qn, int lane, bool barMode, bool isDrums)
    {
        qnLo = std::min(startQN, qn);
        qnHi = std::max(startQN, qn);
        if (barMode)
        {
            laneLo = 0;
            laneHi = isDrums ? 4 : 5;
        }
        else
        {
            laneLo = std::min(startLane, lane);
            laneHi = std::max(startLane, lane);
        }
    }

    bool contains(double qn, int lane) const
    {
        return qn >= qnLo - 0.001 && qn <= qnHi + 0.001
            && lane >= laneLo && lane <= laneHi;
    }
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
    Gem    ghostGem = Gem::NOTE;
    struct StampGhost { int lane; double qnOffset; };
    std::vector<StampGhost> stampGhosts;

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

    // Marquee (select or erase) — highway-space coordinates
    bool        marqueeVisible = false;
    bool        marqueeErase   = false;
    MarqueeRect marqueeRect;
    double      eraseClickedNoteQN    = -1.0;
    int         eraseClickedLane     = -1;
    double      eraseClickedSustainQN = -1.0;
    int         eraseClickedSustainLane = -1;

    bool   barMode = false;
};

//==============================================================================

class OptimisticPatchBuffer
{
public:
    struct Patch {
        int    lane = -1;
        double startQN = 0.0;
        int    framesLeft = 0;
    };

    void addRemove(int lane, double qn) { removes.push_back({ lane, qn, kFrames }); }
    void addAdd(int lane, double qn)    { adds.push_back({ lane, qn, kFrames }); }

    void tick()
    {
        auto expire = [](auto& vec) {
            for (auto& p : vec) --p.framesLeft;
            vec.erase(std::remove_if(vec.begin(), vec.end(),
                [](const auto& p) { return p.framesLeft <= 0; }), vec.end());
        };
        expire(adds);
        expire(removes);
    }

    const std::vector<Patch>& getAdds()    const { return adds; }
    const std::vector<Patch>& getRemoves() const { return removes; }

private:
    std::vector<Patch> adds;
    std::vector<Patch> removes;
    static constexpr int kFrames = 4;
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
    ToggleBarMode,
};

enum class SubMode { Draw, Edit };
enum class DrumDynamic { Normal, Ghost, Accent };
enum class GuitarForce { None, Hopo, Strum, Tap };

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
