#pragma once

#include <JuceHeader.h>
#include "AuthoringTypes.h"
#include "../UI/ControlConstants.h"  // Part enum

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

    // Optional listener fired when any observable state actually changes:
    // writeModeActive, subMode, stepDivision, tuplet, or snapEnabled. Wired by
    // PluginEditor to refresh the toolbar mode pill and write-mode sub-toolbar.
    // Null when unset so the controller stays testable in isolation.
    std::function<void()> onStateChanged;

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
