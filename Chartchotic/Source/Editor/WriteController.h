#pragma once

#include <array>
#include <vector>
#include <JuceHeader.h>
#include "../UI/ControlConstants.h"  // Part enum

//==============================================================================
// Authoring event payloads (M0-G)

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

//==============================================================================
// WriteController — owns write-mode state, receives input, emits overlay state.
// M1.1: skeleton only. Input methods are no-ops; getOverlayState returns default.

enum class SubMode
{
    Draw,
    Edit,
};

class WriteController
{
public:
    explicit WriteController(juce::ValueTree& state);

    // Getters
    bool      writeModeActive() const { return writeModeActiveFlag; }
    SubMode   subMode()         const { return currentSubMode; }
    int       stepDivision()    const { return currentStepDivision; }
    int       tuplet()          const { return currentTuplet; }
    bool      snapEnabled()     const { return snapEnabledFlag; }
    Part      activePart()      const { return currentActivePart; }

    // Setters — only the persisted ones write to ValueTree.
    void setWriteModeActive(bool active);              // transient
    void setSubMode(SubMode mode);                     // persisted
    void setStepDivision(int division);                // persisted (1..64)
    void setTuplet(int t);                             // persisted (0/3/5/7)
    void setSnapEnabled(bool enabled);                 // persisted
    void setActivePart(Part part);                     // transient

    // Controller input methods (M0-G) — no-ops in M1.1.
    void onPointerMove   (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerDown   (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerDrag   (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerUp     (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerExit   ();
    void onPointerCancel ();
    bool onKeyPress      (const juce::KeyPress& key);  // returns false (not consumed)
    void onFrameTick     (double currentProjectQN, bool isPlaying);

    const OverlayState& getOverlayState() const { return overlayState; }

private:
    void loadPersistedState();

    juce::ValueTree& state;

    // Persisted
    SubMode currentSubMode      = SubMode::Draw;
    int     currentStepDivision = 8;       // 1/8 default
    int     currentTuplet       = 0;       // off
    bool    snapEnabledFlag     = true;

    // Transient
    bool writeModeActiveFlag    = false;
    Part currentActivePart      = Part::GUITAR;

    OverlayState overlayState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WriteController)
};
