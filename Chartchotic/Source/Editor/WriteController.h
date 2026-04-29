#pragma once

#include <JuceHeader.h>
#include "AuthoringTypes.h"
#include "../UI/ControlConstants.h"  // Part enum, SkillLevel

// Forward declarations to avoid pulling in MIDI / REAPER headers transitively.
class MidiWriter;
class InstrumentSession;

//==============================================================================
// WriteController — owns write-mode state, receives input, emits overlay state.
// onPointerDown commits left-click placement in Draw mode. Other pointer methods
// are stubbed pending later milestones; getOverlayState returns default until
// hover/drag previews are wired up.

enum class SubMode
{
    Draw,
    Edit,
};

class WriteController
{
public:
    explicit WriteController(juce::ValueTree& state);

    // Post-construction wiring. MidiWriter is owned by ReaperMidiProvider and
    // only exists once REAPER has connected — set/clear via the editor. The
    // InstrumentSession pointer follows the same lifecycle.
    void setMidiWriter(MidiWriter* writer)            { midiWriter = writer; }
    void setInstrumentSession(InstrumentSession* sess){ instrumentSession = sess; }

    // Getters
    bool       writeModeActive() const { return writeModeActiveFlag; }
    SubMode    subMode()         const { return currentSubMode; }
    int        stepDivision()    const { return currentStepDivision; }
    int        tuplet()          const { return currentTuplet; }
    bool       snapEnabled()     const { return snapEnabledFlag; }
    Part       activePart()      const { return currentActivePart; }
    SkillLevel activeSkill()     const { return currentActiveSkill; }

    // Setters — only the persisted ones write to ValueTree.
    void setWriteModeActive(bool active);              // transient
    void setSubMode(SubMode mode);                     // persisted
    void setStepDivision(int division);                // persisted (1..64)
    void setTuplet(int t);                             // persisted (0/3/5/7)
    void setSnapEnabled(bool enabled);                 // persisted
    void setActivePart(Part part);                     // transient
    void setActiveSkill(SkillLevel skill);             // transient

    // Controller input methods. onPointerDown handles left-click placement;
    // the rest are stubs until later milestones.
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
    bool       writeModeActiveFlag = false;
    Part       currentActivePart   = Part::GUITAR;
    SkillLevel currentActiveSkill  = SkillLevel::EXPERT;

    // Non-owning. Lifecycle managed by PluginEditor / ChartchoticAudioProcessor.
    MidiWriter*        midiWriter        = nullptr;
    InstrumentSession* instrumentSession = nullptr;

    OverlayState overlayState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WriteController)
};
