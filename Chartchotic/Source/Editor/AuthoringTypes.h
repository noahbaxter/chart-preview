#pragma once

#include <array>
#include <vector>
#include <JuceHeader.h>

//==============================================================================
// Authoring event payloads (M0-G).
//
// Lightweight types shared between HighwayComponent (event dispatcher) and
// WriteController (state owner). Lives in its own header so HighwayComponent
// doesn't have to pull in the full controller declaration.

struct AuthoringPoint
{
    juce::Point<float> screenPos;
    bool   onHighway = false;        // coupled with laneIndex: true ⇔ laneIndex >= 0
    int    laneIndex = -1;
    double rawProjectQN = 0.0;
    bool   overExistingNote = false;
    double hitNoteStartQN = 0.0;
    int    hitNotePitch = -1;
};

struct AuthoringContext
{
    juce::ModifierKeys mods;
    bool leftButton = false;
    bool rightButton = false;
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

    // Selection
    std::vector<PreviewNote> selectedNotes;

    // Move drag preview
    bool                     moveDragVisible = false;
    std::vector<PreviewNote> movePreviewNotes;

    // Marquee selection
    bool                              marqueeVisible = false;
    std::array<juce::Point<float>, 4> marqueeQuad{};
};

enum class EventType { Down, Drag, Up };

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
    BeginSustain,
    UpdateSustain,
    CommitSustain,
    BeginErase,
    ContinueErase,
    EndErase,
    ToggleBulldoze,
};
