#pragma once

#include <JuceHeader.h>
#include "AuthoringControllerBase.h"

class WriteController : public AuthoringControllerBase
{
public:
    explicit WriteController(juce::ValueTree& state);

    bool       writeModeActive() const { return writeModeActiveFlag; }
    SubMode    subMode()         const { return currentSubMode; }

    void setWriteModeActive(bool active);
    void setSubMode(SubMode mode);
    void setStepDivision(int division);
    void setTuplet(int t);
    void cycleTuplet();
    void setSnapEnabled(bool enabled);

    void onPointerMove   (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerDown   (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerDrag   (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerUp     (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerExit   ()    { lastPointValid = false; recomputeGhost(); }
    void onPointerCancel ()    {}
    void stateDidChange();
    bool onKeyPress      (const juce::KeyPress& key);
    WriteCommand resolveKeyCommand(const juce::KeyPress& key) const
    {
        return commandMapper.resolveKey(writeModeActive(), key);
    }
    void onFrameTick     (double currentProjectQN, bool isPlaying);

private:
    void loadPersistedState();

    juce::ValueTree& state;

    SubMode currentSubMode      = SubMode::Draw;
    bool    writeModeActiveFlag = false;

    AuthoringPoint lastPoint;
    bool           lastPointValid = false;

    // Erase drag state
    bool   eraseDragActive   = false;
    int    eraseDragTrackIdx = -1;
    double eraseLastQN       = 0.0;

    // Sustain drag state
    bool   sustainDragActive   = false;
    int    sustainDragTrackIdx = -1;
    double sustainDragStartQN  = 0.0;
    int    sustainDragLane     = -1;
    int    sustainDragPitch     = -1;
    bool   sustainDragChainMode = false;

    // Paint drag state
    bool   paintDragActive   = false;
    int    paintDragTrackIdx = -1;
    double paintStartQN      = 0.0;
    double paintLastQN        = 0.0;
    int    paintLastLane      = -1;
    struct PaintedNote { double qn; int lane; };
    std::vector<PaintedNote> paintedNotes;
    void paintFillRange(double fromQN, double toQN, int lane);
    void paintShrinkTo(double lo, double hi);

    bool   canWrite(const AuthoringPoint& p) const;
    void   recomputeGhost();
    void   enterSustainDrag(int trackIdx, double startQN, int lane, int pitch, bool chainMode);
    void   clearSustainDrag();

    void handleBeginSustain  (const AuthoringPoint& p, int trackIdx, int pitch, bool drums);
    void handleUpdateSustain (const AuthoringPoint& p);
    void handleCommitSustain (const AuthoringPoint& p);
    void handleBeginPaint    (const AuthoringPoint& p, int trackIdx, int pitch, bool drums);
    void handleContinuePaint (const AuthoringPoint& p);
    void handleCommitPaint   ();
    void handleBeginErase    (const AuthoringPoint& p, int trackIdx, int pitch, bool drums);
    void handleContinueErase (const AuthoringPoint& p);
    void handleEndErase      ();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WriteController)
};
