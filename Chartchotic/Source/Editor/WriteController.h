#pragma once

#include <JuceHeader.h>
#include "AuthoringTypes.h"
#include "CommandMapper.h"
#include "NoteEditor.h"
#include "../UI/ControlConstants.h"

class InstrumentSession;

class WriteController
{
public:
    explicit WriteController(juce::ValueTree& state);

    void setMidiWriter(MidiWriter* writer)           { noteEditor.setMidiWriter(writer); }
    void setInstrumentSession(InstrumentSession* sess){ instrumentSession = sess; noteEditor.setInstrumentSession(sess); }
    void setPlayingStatePtr(const bool* ptr)          { playingStatePtr = ptr; }

    // Getters
    bool       writeModeActive() const { return writeModeActiveFlag; }
    SubMode    subMode()         const { return currentSubMode; }
    int        stepDivision()    const { return currentStepDivision; }
    int        tuplet()          const { return currentTuplet; }
    bool       snapEnabled()     const { return snapEnabledFlag; }
    Part       activePart()      const { return currentActivePart; }
    SkillLevel activeSkill()     const { return currentActiveSkill; }

    // Setters
    void setWriteModeActive(bool active);
    void setSubMode(SubMode mode);
    void setStepDivision(int division);
    void setTuplet(int t);
    void cycleTuplet();
    void setSnapEnabled(bool enabled);
    void setActivePart(Part part)       { currentActivePart = part; }
    void setActiveSkill(SkillLevel s)   { currentActiveSkill = s; }

    // Input
    void onPointerMove   (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerDown   (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerDrag   (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerUp     (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerExit   ()    { lastPointValid = false; recomputeGhost(); }
    void onPointerCancel ()    {}
    bool onKeyPress      (const juce::KeyPress& key);
    void onFrameTick     (double currentProjectQN, bool isPlaying);

    const OverlayState& getOverlayState() const { return overlayState; }

    std::function<void()> onStateChanged;

private:
    void loadPersistedState();
    void stateDidChange();

    juce::ValueTree& state;

    // Persisted
    SubMode currentSubMode      = SubMode::Draw;
    int     currentStepDivision = 8;
    int     currentTuplet       = 0;
    bool    snapEnabledFlag     = true;

    // Transient
    bool       writeModeActiveFlag = false;
    Part       currentActivePart   = Part::GUITAR;
    SkillLevel currentActiveSkill  = SkillLevel::EXPERT;

    InstrumentSession* instrumentSession = nullptr;
    const bool*        playingStatePtr   = nullptr;

    CommandMapper commandMapper;
    NoteEditor    noteEditor;

    OverlayState   overlayState;
    AuthoringPoint lastPoint;
    bool           lastPointValid = false;

    // Erase drag state
    bool eraseDragActive   = false;
    int  eraseDragTrackIdx = -1;

    // Sustain drag state
    bool   sustainDragActive   = false;
    int    sustainDragTrackIdx = -1;
    double sustainDragStartQN  = 0.0;
    int    sustainDragLane     = -1;
    int    sustainDragPitch    = -1;

    // Helpers
    double snapQN(double rawQN) const;
    bool   canWrite(const AuthoringPoint& p) const;
    int    resolvePitch(int laneIndex, bool drums) const;
    int    resolveTrackIdx() const;
    void   recomputeGhost();
    void   enterSustainDrag(int trackIdx, double startQN, int lane, int pitch);
    void   clearSustainDrag();

    // Command handlers — names match WriteCommand enum 1:1
    void handleBeginSustain  (const AuthoringPoint& p, int trackIdx, int pitch, bool drums);
    void handleUpdateSustain (const AuthoringPoint& p);
    void handleCommitSustain (const AuthoringPoint& p);
    void handleBeginErase    (const AuthoringPoint& p, int trackIdx, int pitch, bool drums);
    void handleContinueErase (const AuthoringPoint& p);
    void handleEndErase      ();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WriteController)
};
