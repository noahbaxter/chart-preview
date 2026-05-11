#include "EditController.h"

bool EditController::canEdit(const AuthoringPoint& p) const
{
    return !isPlaying()
        && p.onHighway
        && (barModeFlag || p.laneIndex >= 0)
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

void EditController::onPointerMove(const AuthoringPoint& p, const AuthoringContext&)
{
    updateCursorLabel(p);
}

void EditController::onPointerDown(const AuthoringPoint& p, const AuthoringContext& ctx)
{
    if (!canEdit(p)) return;
    doubleClickConsumed = false;

    auto cmd = commandMapper.resolve(SubMode::Edit, EventType::Down, ctx);
    if (cmd != WriteCommand::SelectAt) return;

    int clickPitch = resolveActivePitch(p.laneIndex);
    int clickLane = barModeFlag ? 0 : p.laneIndex;
    bool hitInteractableNote = p.overExistingNote && !p.hitSustainBody
        && findNote(resolveTrackIdx(), p.hitNoteStartQN, clickPitch).noteIndex >= 0;

    if (hitInteractableNote)
    {
        if (isNoteSelected(p.hitNoteStartQN, clickPitch))
        {
            for (auto& n : selection)
                if (std::abs(n.startQN - p.hitNoteStartQN) < 1e-6 && n.pitch == clickPitch)
                    n.sustainOnly = false;
            recomputeOverlay();
            dragMode = DragMode::Moving;
            moveScreenStart = p.screenPos;
            moveOriginQN = p.rawProjectQN;
            moveOriginLane = clickLane;
            moveAxisLock = ctx.mods.isAltDown();
            moveDragStarted = false;
        }
        else
        {
            int trackIdx = resolveTrackIdx();
            auto found = findNote(trackIdx, p.hitNoteStartQN, clickPitch);

            selection.clear();
            selection.push_back({ trackIdx, found.startQN, found.endQN, clickPitch, clickLane });
            recomputeOverlay();

            pendingSelect = true;
            pendingSelectPoint = p;
            dragMode = DragMode::Moving;
            moveScreenStart = p.screenPos;
            moveOriginQN = p.rawProjectQN;
            moveOriginLane = clickLane;
            moveAxisLock = ctx.mods.isAltDown();
            moveDragStarted = false;
        }
    }
    else
    {
        handleSelectAt(p);
        dragMode = DragMode::Marquee;
        marqueeScreenStart = p.screenPos;
        marqueeRect.begin(p.rawProjectQN, clickLane);
    }
}

void EditController::onPointerDrag(const AuthoringPoint& p, const AuthoringContext& ctx)
{
    if (!canEdit(p)) return;

    float dist = p.screenPos.getDistanceFrom(
        dragMode == DragMode::Moving ? moveScreenStart : marqueeScreenStart);

    if (dist < kDragThresholdPx) return;

    if (pendingSelect)
    {
        pendingSelect = false;
        doubleClickConsumed = true;
    }

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
    overlayState.ghostVisible = false;
    overlayState.ghostLane = -1;
    overlayState.ghostQN = 0.0;
}

bool EditController::onKeyPress(const juce::KeyPress& key)
{
    int code = key.getKeyCode();
    if (!selection.empty())
    {
        double step = stepSpacingQN(currentStepDivision, currentTuplet);
        if (code == juce::KeyPress::upKey)    { handleArrowMove(0,  step); return true; }
        if (code == juce::KeyPress::downKey)  { handleArrowMove(0, -step); return true; }
        if (code == juce::KeyPress::leftKey)  { handleArrowMove(-1, 0.0);  return true; }
        if (code == juce::KeyPress::rightKey) { handleArrowMove(1,  0.0);  return true; }
    }

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
        int lane = barModeFlag ? 0 : p.laneIndex;
        int pitch = resolveActivePitch(p.laneIndex);

        if (isNoteSelected(p.hitNoteStartQN, pitch))
            return;

        bool sustainOnly = p.hitSustainBody;
        if (sustainOnly)
        {
            auto info = findNote(trackIdx, p.hitNoteStartQN, pitch);
            if (info.noteIndex < 0 || (info.endQN - info.startQN) < double(MIDI_MIN_SUSTAIN_LENGTH))
                sustainOnly = false;
        }

        auto found = findNote(trackIdx, p.hitNoteStartQN, pitch);
        if (found.noteIndex < 0)
        {
            selection.clear();
            recomputeOverlay();
            if (onStateChanged) onStateChanged();
            return;
        }

        selection.clear();
        selection.push_back({ trackIdx, found.startQN, found.endQN, pitch, lane, sustainOnly });
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
    marqueeRect.update(p.rawProjectQN, p.laneIndex, barModeFlag, isDrums());
    overlayState.marqueeVisible = true;
    overlayState.marqueeErase   = false;
    overlayState.marqueeRect    = marqueeRect;
    if (onStateChanged) onStateChanged();
}

void EditController::handleCommitMarquee(const AuthoringPoint& p)
{
    int trackIdx = resolveTrackIdx();
    if (trackIdx < 0) return;

    marqueeRect.update(p.rawProjectQN, p.laneIndex, barModeFlag, isDrums());

    selection.clear();
    for (const auto& cn : classifyNotesInRect(trackIdx, marqueeRect))
        selection.push_back({ trackIdx, cn.note.startQN, cn.note.endQN, cn.note.pitch, cn.lane, cn.sustainOnly });

    recomputeOverlay();
    if (onStateChanged) onStateChanged();
}

void EditController::handleContinueMove(const AuthoringPoint& p)
{
    moveDragStarted = true;

    double deltaQN = p.rawProjectQN - moveOriginQN;
    int deltaLane = barModeFlag ? 0 : (p.laneIndex - moveOriginLane);

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
        auto info = findNote(n.trackIdx, n.startQN, n.pitch);
        double duration = (info.noteIndex >= 0) ? (info.endQN - info.startQN) : 0.1;
        overlayState.movePreviewNotes.push_back({ newLane, newQN, newQN + duration, newPitch });
    }

    if (onStateChanged) onStateChanged();
}

void EditController::handleCommitMove(const AuthoringPoint& p)
{
    if (selection.empty()) return;

    double deltaQN = p.rawProjectQN - moveOriginQN;
    int deltaLane = barModeFlag ? 0 : (p.laneIndex - moveOriginLane);

    if (!barModeFlag && moveAxisLock)
    {
        if (std::abs(deltaQN) > std::abs(deltaLane * 0.5))
            deltaLane = 0;
        else
            deltaQN = 0.0;
    }

    bool drums = isDrums();
    int maxLane = drums ? 4 : 5;

    beginBatch("Chartchotic: Move notes");

    std::vector<SelectedNote> moved;
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
        moved.push_back({ n.trackIdx, newStartQN, newEndQN, newPitch, newLane });
    }

    endBatch();
    selection = std::move(moved);
    recomputeOverlay();
    if (onStateChanged) onStateChanged();
}

void EditController::handleDoubleClick(const AuthoringPoint& p)
{
    int trackIdx = resolveTrackIdx();
    if (trackIdx < 0) return;

    bool drums = isDrums();

    if (barModeFlag)
    {
        int barPitch = resolveBarPitch();
        if (p.overExistingNote)
        {
            auto existing = findNote(trackIdx, p.hitNoteStartQN, barPitch);
            if (existing.noteIndex >= 0)
                eraseBarNote(trackIdx, existing.startQN);
        }
        else
            createBarNote(trackIdx, snapQN(p.rawProjectQN));
    }
    else if (p.overExistingNote)
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
        {
            createNote(trackIdx, qn, pitch, p.laneIndex, resolveVelocity());
            if (drums)
                writeTomMarker(trackIdx, qn, p.laneIndex);
            else
                writeGuitarForceMarker(trackIdx, qn);
        }
    }

    recomputeOverlay();
    if (onStateChanged) onStateChanged();
}

void EditController::handleDeleteSelection()
{
    if (selection.empty()) return;

    beginBatch("Chartchotic: Delete notes");
    for (const auto& n : selection)
    {
        if (n.sustainOnly)
            truncateNote(n.trackIdx, n.startQN, n.pitch);
        else
            eraseNote(n.trackIdx, n.startQN, n.pitch, isDrums(), n.lane, currentActiveSkill);
    }
    endBatch();

    selection.clear();
    recomputeOverlay();
    if (onStateChanged) onStateChanged();
}

void EditController::handleArrowMove(int deltaLane, double deltaQN)
{
    if (selection.empty()) return;
    if (barModeFlag) deltaLane = 0;

    bool drums = isDrums();
    int maxLane = drums ? 4 : 5;

    for (const auto& n : selection)
    {
        int newLane = n.lane + deltaLane;
        if (newLane < 0 || newLane > maxLane) return;
        if (n.startQN + deltaQN < 0.0) return;
    }

    beginBatch("Chartchotic: Move notes");

    std::vector<SelectedNote> moved;
    for (const auto& n : selection)
    {
        auto found = findNote(n.trackIdx, n.startQN, n.pitch);
        if (found.noteIndex < 0) continue;

        int newLane = n.lane + deltaLane;
        double newStartQN = n.startQN + deltaQN;
        double duration = found.endQN - found.startQN;
        int newPitch = resolvePitch(newLane, drums);

        double newEndQN = newStartQN + duration;
        moveNote(n.trackIdx, n.startQN, n.pitch, n.lane,
                 newStartQN, newEndQN, newPitch, newLane);
        moved.push_back({ n.trackIdx, newStartQN, newEndQN, newPitch, newLane });
    }

    endBatch();
    selection = std::move(moved);
    recomputeOverlay();
    if (onStateChanged) onStateChanged();
}

void EditController::applyDrumDynamicToSelection(DrumDynamic dynamic)
{
    if (selection.empty() || !noteEditorAvailable()) return;
    int trackIdx = resolveTrackIdx();
    if (trackIdx < 0) return;

    int velocity = 100;
    if (dynamic == DrumDynamic::Ghost) velocity = 1;
    else if (dynamic == DrumDynamic::Accent) velocity = 127;

    beginBatch("Set drum dynamic");
    for (const auto& sel : selection)
    {
        if (sel.sustainOnly) continue;
        setNoteVelocity(trackIdx, sel.startQN, sel.pitch, velocity);
    }
    endBatch();
}

void EditController::applyGuitarForceToSelection(GuitarForce force)
{
    if (selection.empty() || !noteEditorAvailable()) return;
    int trackIdx = resolveTrackIdx();
    if (trackIdx < 0) return;

    beginBatch("Set guitar force");
    for (const auto& sel : selection)
    {
        if (sel.sustainOnly) continue;

        int hopoPitch = resolveGuitarForcePitchFor(GuitarForce::Hopo);
        int strumPitch = resolveGuitarForcePitchFor(GuitarForce::Strum);
        int tapPitch = (int)MidiPitchDefinitions::Guitar::TAP;

        for (int fp : {hopoPitch, strumPitch, tapPitch})
        {
            if (fp < 0) continue;
            auto existing = findNote(trackIdx, sel.startQN, fp);
            if (existing.noteIndex >= 0)
                eraseNote(trackIdx, sel.startQN, fp, false, sel.lane, currentActiveSkill);
        }

        if (force != GuitarForce::None)
        {
            GuitarForce saved = currentGuitarForce;
            currentGuitarForce = force;
            writeGuitarForceMarker(trackIdx, sel.startQN);
            currentGuitarForce = saved;
        }
    }
    endBatch();
}

void EditController::applyCymbalModeToSelection(bool cymbal)
{
    if (selection.empty() || !noteEditorAvailable()) return;
    int trackIdx = resolveTrackIdx();
    if (trackIdx < 0) return;

    beginBatch("Set cymbal/tom");
    for (const auto& sel : selection)
    {
        if (sel.sustainOnly) continue;
        int markerPitch = resolveTomMarkerPitch(sel.lane);
        if (markerPitch < 0) continue;

        auto existing = findNote(trackIdx, sel.startQN, markerPitch);
        if (cymbal && existing.noteIndex >= 0)
            eraseNote(trackIdx, sel.startQN, markerPitch, true, sel.lane, currentActiveSkill);
        else if (!cymbal && existing.noteIndex < 0)
            createMarkerNote(trackIdx, sel.startQN, markerPitch);
    }
    endBatch();
}

void EditController::updateCursorLabel(const AuthoringPoint& p)
{
    overlayState.ghostVisible = false;
    overlayState.ghostLane    = -1;
    overlayState.ghostQN      = 0.0;

    if (isPlaying()) return;
    if (!p.onHighway || p.laneIndex < 0) return;

    double qn = snapQN(p.rawProjectQN);
    if (qn < 0.0) qn = 0.0;

    overlayState.ghostVisible = true;
    overlayState.ghostQN      = qn;
}
