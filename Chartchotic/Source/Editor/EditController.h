#pragma once

#include <JuceHeader.h>
#include "AuthoringControllerBase.h"

class EditController : public AuthoringControllerBase
{
public:
    EditController() = default;

    void onPointerMove       (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerDown       (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerDrag       (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerUp         (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerDoubleClick(const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerExit();
    bool onKeyPress(const juce::KeyPress& key);

    void clearSelection();
    void onFrameTick();

private:
    enum class DragMode { Idle, Marquee, Moving };

    bool   canEdit(const AuthoringPoint& p) const;
    bool   isNoteSelected(double startQN, int pitch) const;
    void   recomputeOverlay();

    void handleSelectAt       (const AuthoringPoint& p);
    void handleContinueMarquee(const AuthoringPoint& p);
    void handleCommitMarquee  (const AuthoringPoint& p);
    void handleContinueMove   (const AuthoringPoint& p);
    void handleCommitMove     (const AuthoringPoint& p);
    void handleDoubleClick    (const AuthoringPoint& p);
    void handleDeleteSelection();
    void updateCursorLabel(const AuthoringPoint& p);

    // Selection
    std::vector<SelectedNote> selection;

    // Drag state
    DragMode dragMode = DragMode::Idle;
    static constexpr float kDragThresholdPx = 4.0f;

    // Marquee state
    juce::Point<float> marqueeScreenStart;
    MarqueeRect marqueeRect;

    // Move state
    juce::Point<float> moveScreenStart;
    double moveOriginQN   = 0.0;
    int    moveOriginLane  = 0;
    bool   moveAxisLock    = false;
    bool   moveDragStarted = false;

    bool   doubleClickConsumed = false;
    bool   pendingSelect = false;
    AuthoringPoint pendingSelectPoint;

    std::shared_ptr<bool> aliveFlag = std::make_shared<bool>(true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditController)
};
