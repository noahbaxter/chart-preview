#include "InteractionController.h"

InteractionController::InteractionController(juce::ValueTree& state)
    : writeController(state)
{
    writeController.onStateChanged = [this]() {
        if (writeController.subMode() == SubMode::Draw)
            editController.clearSelection();
        if (onStateChanged) onStateChanged();
    };

    editController.onStateChanged = [this]() {
        if (onStateChanged) onStateChanged();
    };
}

bool InteractionController::isEditActive() const
{
    return writeController.writeModeActive()
        && writeController.subMode() == SubMode::Edit;
}

void InteractionController::onFrame(MidiWriter* writer, InstrumentSession* session,
                                    const bool* playingState, Part activePart, SkillLevel activeSkill,
                                    double currentProjectQN)
{
    writeController.setMidiWriter(writer);
    writeController.setInstrumentSession(session);
    writeController.setPlayingStatePtr(playingState);
    writeController.setActivePart(activePart);
    writeController.setActiveSkill(activeSkill);
    writeController.setPatchBuffer(&patchBuffer);

    editController.setMidiWriter(writer);
    editController.setInstrumentSession(session);
    editController.setPlayingStatePtr(playingState);
    editController.setActivePart(activePart);
    editController.setActiveSkill(activeSkill);
    editController.setStepDivision(writeController.stepDivision());
    editController.setTuplet(writeController.tuplet());
    editController.setSnapEnabled(writeController.snapEnabled());
    editController.setPatchBuffer(&patchBuffer);

    patchBuffer.tick();
    bool playing = playingState ? *playingState : false;
    writeController.onFrameTick(currentProjectQN, playing);
    editController.onFrameTick();
}

void InteractionController::onPointerMove(const AuthoringPoint& p, const AuthoringContext& ctx)
{
    if (isEditActive()) editController.onPointerMove(p, ctx);
    else                writeController.onPointerMove(p, ctx);
}

void InteractionController::onPointerDown(const AuthoringPoint& p, const AuthoringContext& ctx)
{
    if (isEditActive()) editController.onPointerDown(p, ctx);
    else                writeController.onPointerDown(p, ctx);
}

void InteractionController::onPointerDrag(const AuthoringPoint& p, const AuthoringContext& ctx)
{
    if (isEditActive()) editController.onPointerDrag(p, ctx);
    else                writeController.onPointerDrag(p, ctx);
}

void InteractionController::onPointerUp(const AuthoringPoint& p, const AuthoringContext& ctx)
{
    if (isEditActive()) editController.onPointerUp(p, ctx);
    else                writeController.onPointerUp(p, ctx);
}

void InteractionController::onPointerDoubleClick(const AuthoringPoint& p, const AuthoringContext& ctx)
{
    if (isEditActive())
        editController.onPointerDoubleClick(p, ctx);
}

void InteractionController::onPointerExit()
{
    writeController.onPointerExit();
    editController.onPointerExit();
}

void InteractionController::onPointerCancel()
{
    writeController.onPointerCancel();
}

bool InteractionController::onKeyPress(const juce::KeyPress& key)
{
    if (isEditActive())
    {
        if (editController.onKeyPress(key))
            return true;
    }
    return writeController.onKeyPress(key);
}

const OverlayState& InteractionController::getOverlayState() const
{
    if (isEditActive())
        return editController.getOverlayState();
    return writeController.getOverlayState();
}
