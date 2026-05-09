#include "EditController.h"

bool EditController::canEdit(const AuthoringPoint& p) const
{
    return !isPlaying()
        && p.onHighway
        && p.laneIndex >= 0
        && noteEditorAvailable()
        && instrumentSession != nullptr;
}

bool EditController::isNoteSelected(double startQN, int pitch) const
{
    for (const auto& n : selection)
        if (std::abs(n.startQN - startQN) < 1e-6 && n.pitch == pitch)
            return true;
    return false;
}

void EditController::clearSelection()
{
    selection.clear();
    recomputeOverlay();
    if (onStateChanged) onStateChanged();
}

void EditController::onFrameTick()
{
}

void EditController::recomputeOverlay()
{
    overlayState.selectedNotes.clear();
    overlayState.selectionBoundsVisible = false;

    for (const auto& n : selection)
        overlayState.selectedNotes.push_back(n);

    if (!selection.empty())
    {
        auto& b = overlayState.selectionBounds;
        b.minLane = selection[0].lane;
        b.maxLane = selection[0].lane;
        b.minQN   = selection[0].startQN;
        b.maxQN   = selection[0].startQN;
        for (const auto& n : selection)
        {
            b.minLane = std::min(b.minLane, n.lane);
            b.maxLane = std::max(b.maxLane, n.lane);
            b.minQN   = std::min(b.minQN, n.startQN);
            b.maxQN   = std::max(b.maxQN, n.startQN);
        }
        overlayState.selectionBoundsVisible = true;
    }
}

//==============================================================================
// Input handlers

void EditController::onPointerMove(const AuthoringPoint&, const AuthoringContext&)
{
}

void EditController::onPointerDown(const AuthoringPoint& p, const AuthoringContext& ctx)
{
    if (!canEdit(p)) return;
    doubleClickConsumed = false;

    auto cmd = commandMapper.resolve(SubMode::Edit, EventType::Down, ctx);
    if (cmd != WriteCommand::SelectAt) return;

    if (p.overExistingNote)
    {
        bool drums = isDrums();
        int clickPitch = resolvePitch(p.laneIndex, drums);

        if (isNoteSelected(p.hitNoteStartQN, clickPitch))
        {
            dragMode = DragMode::Moving;
            moveScreenStart = p.screenPos;
            moveOriginQN = p.rawProjectQN;
            moveOriginLane = p.laneIndex;
            moveAxisLock = ctx.mods.isAltDown();
            moveDragStarted = false;
        }
        else
        {
            pendingSelect = true;
            pendingSelectPoint = p;
            dragMode = DragMode::Moving;
            moveScreenStart = p.screenPos;
            moveOriginQN = p.rawProjectQN;
            moveOriginLane = p.laneIndex;
            moveAxisLock = ctx.mods.isAltDown();
            moveDragStarted = false;
        }
    }
    else
    {
        handleSelectAt(p);
        dragMode = DragMode::Marquee;
        marqueeScreenStart = p.screenPos;
        marqueeLaneStart = p.laneIndex;
        marqueeQNStart = p.rawProjectQN;
    }
}

void EditController::onPointerDrag(const AuthoringPoint& p, const AuthoringContext& ctx)
{
    if (!canEdit(p)) return;

    float dist = p.screenPos.getDistanceFrom(
        dragMode == DragMode::Moving ? moveScreenStart : marqueeScreenStart);

    if (dist < kDragThresholdPx) return;

    if (dragMode == DragMode::Marquee)
        handleContinueMarquee(p);
    else if (dragMode == DragMode::Moving)
    {
        moveAxisLock = ctx.mods.isAltDown();
        handleContinueMove(p);
    }
}

void EditController::onPointerUp(const AuthoringPoint& p, const AuthoringContext&)
{
    if (dragMode == DragMode::Marquee)
        handleCommitMarquee(p);
    else if (dragMode == DragMode::Moving && moveDragStarted)
        handleCommitMove(p);

    if (pendingSelect && !doubleClickConsumed)
    {
        auto pt = pendingSelectPoint;
        pendingSelect = false;
        std::weak_ptr<bool> weak = aliveFlag;
        juce::Timer::callAfterDelay(
            juce::MouseEvent::getDoubleClickTimeout(),
            [this, pt, weak]() {
                if (weak.expired()) return;
                if (doubleClickConsumed) return;
                handleSelectAt(pt);
                if (onStateChanged) onStateChanged();
            });
    }

    dragMode = DragMode::Idle;
    overlayState.marqueeVisible = false;
    overlayState.moveDragVisible = false;
    overlayState.movePreviewNotes.clear();
    if (onStateChanged) onStateChanged();
}

void EditController::onPointerDoubleClick(const AuthoringPoint& p, const AuthoringContext& ctx)
{
    if (!canEdit(p)) return;
    doubleClickConsumed = true;
    pendingSelect = false;
    auto cmd = commandMapper.resolve(SubMode::Edit, EventType::DoubleClick, ctx);
    if (cmd == WriteCommand::DoubleClick)
        handleDoubleClick(p);
}

void EditController::onPointerExit()
{
}

bool EditController::onKeyPress(const juce::KeyPress& key)
{
    auto cmd = commandMapper.resolveKey(true, key);
    switch (cmd)
    {
        case WriteCommand::DeleteSelection: handleDeleteSelection(); return true;
        case WriteCommand::DeselectAll:     clearSelection();        return true;
        default: return false;
    }
}

//==============================================================================
// Command handlers

void EditController::handleSelectAt(const AuthoringPoint& p)
{
    if (p.overExistingNote)
    {
        int trackIdx = resolveTrackIdx();
        bool drums = isDrums();
        int lane = p.laneIndex;
        int pitch = resolvePitch(lane, drums);

        if (isNoteSelected(p.hitNoteStartQN, pitch))
            return;

        selection.clear();
        selection.push_back({ trackIdx, p.hitNoteStartQN, pitch, lane });
    }
    else
    {
        selection.clear();
    }
    recomputeOverlay();
    if (onStateChanged) onStateChanged();
}

void EditController::handleContinueMarquee(const AuthoringPoint& p)
{
    overlayState.marqueeVisible = true;
    overlayState.marqueeLaneStart = std::min(marqueeLaneStart, p.laneIndex);
    overlayState.marqueeLaneEnd   = std::max(marqueeLaneStart, p.laneIndex);
    overlayState.marqueeQNStart   = std::min(marqueeQNStart, p.rawProjectQN);
    overlayState.marqueeQNEnd     = std::max(marqueeQNStart, p.rawProjectQN);
    if (onStateChanged) onStateChanged();
}

void EditController::handleCommitMarquee(const AuthoringPoint& p)
{
    int trackIdx = resolveTrackIdx();
    if (trackIdx < 0) return;

    bool drums = isDrums();

    int laneMin = std::min(marqueeLaneStart, p.laneIndex);
    int laneMax = std::max(marqueeLaneStart, p.laneIndex);
    double qnMin = std::min(marqueeQNStart, p.rawProjectQN);
    double qnMax = std::max(marqueeQNStart, p.rawProjectQN);

    selection.clear();
    for (int lane = laneMin; lane <= laneMax; ++lane)
    {
        int pitch = resolvePitch(lane, drums);
        auto notes = findNotesInRange(trackIdx, qnMin, qnMax, pitch);
        for (const auto& note : notes)
            selection.push_back({ trackIdx, note.startQN, note.pitch, lane });
    }

    recomputeOverlay();
    if (onStateChanged) onStateChanged();
}

void EditController::handleContinueMove(const AuthoringPoint& p)
{
    moveDragStarted = true;

    double deltaQN = p.rawProjectQN - moveOriginQN;
    int deltaLane = p.laneIndex - moveOriginLane;

    if (moveAxisLock)
    {
        if (std::abs(deltaQN) > std::abs(deltaLane * 0.5))
            deltaLane = 0;
        else
            deltaQN = 0.0;
    }

    bool drums = isDrums();
    int maxLane = drums ? 4 : 5;

    overlayState.moveDragVisible = true;
    overlayState.movePreviewNotes.clear();
    for (const auto& n : selection)
    {
        int newLane = juce::jlimit(0, maxLane, n.lane + deltaLane);
        double newQN = snapQN(n.startQN + deltaQN);
        if (newQN < 0.0) newQN = 0.0;
        int newPitch = resolvePitch(newLane, drums);
        overlayState.movePreviewNotes.push_back({ newLane, newQN, newQN + 0.1, newPitch });
    }

    if (onStateChanged) onStateChanged();
}

void EditController::handleCommitMove(const AuthoringPoint& p)
{
    if (selection.empty()) return;

    double deltaQN = p.rawProjectQN - moveOriginQN;
    int deltaLane = p.laneIndex - moveOriginLane;

    if (moveAxisLock)
    {
        if (std::abs(deltaQN) > std::abs(deltaLane * 0.5))
            deltaLane = 0;
        else
            deltaQN = 0.0;
    }

    bool drums = isDrums();
    int maxLane = drums ? 4 : 5;

    beginBatch("Chartchotic: Move notes");

    for (const auto& n : selection)
    {
        int newLane = juce::jlimit(0, maxLane, n.lane + deltaLane);
        double newStartQN = snapQN(n.startQN + deltaQN);
        if (newStartQN < 0.0) newStartQN = 0.0;

        auto found = findNote(n.trackIdx, n.startQN, n.pitch);
        if (found.noteIndex < 0) continue;

        double duration = found.endQN - found.startQN;
        double newEndQN = newStartQN + duration;
        int newPitch = resolvePitch(newLane, drums);

        moveNote(n.trackIdx, n.startQN, n.pitch, n.lane,
                 newStartQN, newEndQN, newPitch, newLane);
    }

    endBatch();
    selection.clear();
    recomputeOverlay();
    if (onStateChanged) onStateChanged();
}

void EditController::handleDoubleClick(const AuthoringPoint& p)
{
    int trackIdx = resolveTrackIdx();
    if (trackIdx < 0) return;

    bool drums = isDrums();

    if (p.overExistingNote)
    {
        int pitch = resolvePitch(p.laneIndex, drums);
        eraseNote(trackIdx, p.hitNoteStartQN, pitch, drums, p.laneIndex, currentActiveSkill);
        selection.erase(
            std::remove_if(selection.begin(), selection.end(),
                [&](const SelectedNote& n) {
                    return std::abs(n.startQN - p.hitNoteStartQN) < 1e-6
                        && n.pitch == pitch;
                }),
            selection.end());
    }
    else
    {
        double qn = snapQN(p.rawProjectQN);
        int pitch = resolvePitch(p.laneIndex, drums);
        auto existing = findNote(trackIdx, qn, pitch);
        if (existing.noteIndex >= 0)
            eraseNote(trackIdx, qn, pitch, drums, p.laneIndex, currentActiveSkill);
        else
            createNote(trackIdx, qn, pitch, p.laneIndex);
    }

    recomputeOverlay();
    if (onStateChanged) onStateChanged();
}

void EditController::handleDeleteSelection()
{
    if (selection.empty()) return;

    beginBatch("Chartchotic: Delete notes");
    for (const auto& n : selection)
        eraseNote(n.trackIdx, n.startQN, n.pitch, isDrums(), n.lane, currentActiveSkill);
    endBatch();

    selection.clear();
    recomputeOverlay();
    if (onStateChanged) onStateChanged();
}
