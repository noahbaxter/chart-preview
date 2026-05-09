#pragma once

#include <JuceHeader.h>
#include "AuthoringTypes.h"
#include "AuthoringUtils.h"
#include "CommandMapper.h"
#include "NoteEditor.h"
#include "../UI/ControlConstants.h"

class InstrumentSession;

class EditController
{
public:
    EditController() = default;

    void setMidiWriter(MidiWriter* writer)            { noteEditor.setMidiWriter(writer); }
    void setInstrumentSession(InstrumentSession* sess) { instrumentSession = sess; noteEditor.setInstrumentSession(sess); }
    void setPlayingStatePtr(const bool* ptr)           { playingStatePtr = ptr; }

    void setStepDivision(int d)       { currentStepDivision = d; }
    void setTuplet(int t)             { currentTuplet = t; }
    void setSnapEnabled(bool s)       { snapEnabledFlag = s; }
    void setActivePart(Part p)        { currentActivePart = p; }
    void setActiveSkill(SkillLevel s) { currentActiveSkill = s; }

    void onPointerMove       (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerDown       (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerDrag       (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerUp         (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerDoubleClick(const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerExit();
    bool onKeyPress(const juce::KeyPress& key);

    void clearSelection();

    const OverlayState& getOverlayState() const { return overlayState; }

    std::function<void()> onStateChanged;

private:
    enum class DragMode { Idle, Marquee, Moving };

    bool   canEdit(const AuthoringPoint& p) const;
    int    resolvePitch(int laneIndex, bool drums) const;
    int    resolveTrackIdx() const;
    double snap(double rawQN) const;
    bool   isNoteSelected(double startQN, int pitch) const;
    void   recomputeOverlay();

    void handleSelectAt       (const AuthoringPoint& p);
    void handleContinueMarquee(const AuthoringPoint& p);
    void handleCommitMarquee  (const AuthoringPoint& p);
    void handleContinueMove   (const AuthoringPoint& p);
    void handleCommitMove     (const AuthoringPoint& p);
    void handleDoubleClick    (const AuthoringPoint& p);
    void handleDeleteSelection();

    InstrumentSession* instrumentSession = nullptr;
    const bool*        playingStatePtr   = nullptr;

    CommandMapper commandMapper;
    NoteEditor    noteEditor;
    OverlayState  overlayState;

    Part       currentActivePart   = Part::GUITAR;
    SkillLevel currentActiveSkill  = SkillLevel::EXPERT;
    int        currentStepDivision = 8;
    int        currentTuplet       = 0;
    bool       snapEnabledFlag     = true;

    // Selection
    std::vector<SelectedNote> selection;

    // Drag state
    DragMode dragMode = DragMode::Idle;
    static constexpr float kDragThresholdPx = 4.0f;

    // Marquee state
    juce::Point<float> marqueeScreenStart;
    int    marqueeLaneStart = 0;
    double marqueeQNStart   = 0.0;

    // Move state
    juce::Point<float> moveScreenStart;
    double moveOriginQN   = 0.0;
    int    moveOriginLane  = 0;
    bool   moveAxisLock    = false;
    bool   moveDragStarted = false;

    bool   doubleClickConsumed = false;
    bool   pendingSelect = false;
    AuthoringPoint pendingSelectPoint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditController)
};
