#include "WriteController.h"
#include "../Midi/Utils/MidiConstants.h"

namespace
{
    const juce::Identifier kWriteSubMode      { "writeSubMode" };
    const juce::Identifier kWriteStepDivision { "writeStepDivision" };
    const juce::Identifier kWriteTuplet       { "writeTuplet" };
    const juce::Identifier kWriteSnap         { "writeSnap" };

    static const juce::String kSubModeDraw { "draw" };
    static const juce::String kSubModeEdit { "edit" };

    SubMode parseSubMode(const juce::String& s, SubMode fallback)
    {
        if (s == kSubModeEdit) return SubMode::Edit;
        if (s == kSubModeDraw) return SubMode::Draw;
        return fallback;
    }

    const juce::String& subModeToString(SubMode m)
    {
        return (m == SubMode::Edit) ? kSubModeEdit : kSubModeDraw;
    }
}

//==============================================================================

WriteController::WriteController(juce::ValueTree& st)
    : state(st)
{
    loadPersistedState();
}

void WriteController::loadPersistedState()
{
    if (state.hasProperty(kWriteSubMode))
        currentSubMode = parseSubMode(state.getProperty(kWriteSubMode).toString(), currentSubMode);

    if (state.hasProperty(kWriteStepDivision))
        currentStepDivision = juce::jlimit(1, 64, (int)state.getProperty(kWriteStepDivision));

    if (state.hasProperty(kWriteTuplet))
    {
        int t = (int)state.getProperty(kWriteTuplet);
        if (t == 0 || t == 3 || t == 5 || t == 7)
            currentTuplet = t;
    }

    if (state.hasProperty(kWriteSnap))
        snapEnabledFlag = (bool)state.getProperty(kWriteSnap);
}

//==============================================================================

bool WriteController::canWrite(const AuthoringPoint& p) const
{
    return writeModeActive()
        && !isPlaying()
        && currentSubMode == SubMode::Draw
        && p.onHighway
        && p.laneIndex >= 0
        && noteEditorAvailable()
        && instrumentSession != nullptr;
}

void WriteController::stateDidChange()
{
    recomputeGhost();
    if (onStateChanged) onStateChanged();
}

void WriteController::recomputeGhost()
{
    overlayState.ghostVisible    = false;
    overlayState.ghostLane       = -1;
    overlayState.ghostQN         = 0.0;
    overlayState.ghostShowsErase = false;

    if (!lastPointValid)                                  return;
    if (!writeModeActive())                               return;
    if (isPlaying())                                      return;
    if (currentSubMode != SubMode::Draw)                  return;
    if (!lastPoint.onHighway || lastPoint.laneIndex < 0)  return;
    if (sustainDragActive || paintDragActive || eraseDragActive) return;

    double qn = snapQN(lastPoint.rawProjectQN);
    if (qn < 0.0) qn = 0.0;

    overlayState.ghostVisible = true;
    overlayState.ghostLane    = lastPoint.laneIndex;
    overlayState.ghostQN      = qn;
}

void WriteController::enterSustainDrag(int trackIdx, double startQN, int lane, int pitch, bool chainMode)
{
    sustainDragActive    = true;
    sustainDragTrackIdx  = trackIdx;
    sustainDragStartQN   = startQN;
    sustainDragLane      = lane;
    sustainDragPitch     = pitch;
    sustainDragChainMode = chainMode;
}

void WriteController::clearSustainDrag()
{
    sustainDragActive    = false;
    sustainDragTrackIdx  = -1;
    sustainDragChainMode = false;
    overlayState.drawPreviewVisible = false;
    overlayState.drawPreviewNotes.clear();
}

//==============================================================================
// Setters

void WriteController::setWriteModeActive(bool active)
{
    if (writeModeActiveFlag == active) return;
    writeModeActiveFlag = active;
    stateDidChange();
}

void WriteController::setSubMode(SubMode mode)
{
    if (currentSubMode == mode) return;
    currentSubMode = mode;
    state.setProperty(kWriteSubMode, subModeToString(mode), nullptr);
    stateDidChange();
}

void WriteController::setStepDivision(int division)
{
    int clamped = juce::jlimit(1, 64, division);
    if (currentStepDivision == clamped) return;
    currentStepDivision = clamped;
    state.setProperty(kWriteStepDivision, clamped, nullptr);
    stateDidChange();
}

void WriteController::setTuplet(int t)
{
    if (t != 0 && t != 3 && t != 5 && t != 7) return;
    if (currentTuplet == t) return;
    currentTuplet = t;
    state.setProperty(kWriteTuplet, t, nullptr);
    stateDidChange();
}

void WriteController::cycleTuplet()
{
    switch (currentTuplet)
    {
        case 0: setTuplet(3); break;
        case 3: setTuplet(5); break;
        case 5: setTuplet(7); break;
        default: setTuplet(0); break;
    }
}

void WriteController::setSnapEnabled(bool enabled)
{
    if (snapEnabledFlag == enabled) return;
    snapEnabledFlag = enabled;
    state.setProperty(kWriteSnap, enabled, nullptr);
    stateDidChange();
}

//==============================================================================
// Input dispatch

void WriteController::onPointerMove(const AuthoringPoint& p,
                                    [[maybe_unused]] const AuthoringContext& ctx)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    lastPoint = p;
    lastPointValid = true;
    recomputeGhost();
}

void WriteController::onPointerDown(const AuthoringPoint& p, const AuthoringContext& ctx)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    if (!canWrite(p)) return;

    int trackIdx = resolveTrackIdx();
    if (trackIdx < 0) return;

    bool drums = isDrums();
    int  pitch = resolvePitch(p.laneIndex, drums);
    if (pitch < 0) return;

    auto cmd = commandMapper.resolve(currentSubMode, EventType::Down, ctx);

    switch (cmd)
    {
        case WriteCommand::BeginSustain: handleBeginSustain(p, trackIdx, pitch, drums); break;
        case WriteCommand::BeginPaint:   handleBeginPaint(p, trackIdx, pitch, drums);   break;
        case WriteCommand::BeginErase:   handleBeginErase(p, trackIdx, pitch, drums);   break;
        default: break;
    }
}

void WriteController::onPointerDrag(const AuthoringPoint& p,
                                    [[maybe_unused]] const AuthoringContext& ctx)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    if (sustainDragActive)   { handleUpdateSustain(p);  return; }
    if (paintDragActive)     { handleContinuePaint(p);  return; }
    if (eraseDragActive)     { handleContinueErase(p);  return; }
}

void WriteController::onPointerUp(const AuthoringPoint& p,
                                  [[maybe_unused]] const AuthoringContext& ctx)
{
    if (sustainDragActive)   { handleCommitSustain(p);  return; }
    if (paintDragActive)     { handleCommitPaint();     return; }
    if (eraseDragActive)     { handleEndErase();        return; }
}

void WriteController::onFrameTick([[maybe_unused]] double currentProjectQN,
                                  [[maybe_unused]] bool isPlaying)
{
    recomputeGhost();
}

//==============================================================================
// Sustain command handlers

void WriteController::handleBeginSustain(const AuthoringPoint& p, int trackIdx, int pitch, bool drums)
{
    double clickQN = snapQN(p.rawProjectQN);
    bool onExistingNote = p.overExistingNote
        && std::abs(snapQN(p.hitNoteStartQN) - clickQN) < 0.001;

    if (!drums)
        beginBatch("Chartchotic: Sustain note");

    if (!onExistingNote)
    {
        createNote(trackIdx, clickQN, pitch, p.laneIndex);
        if (drums) return;
        enterSustainDrag(trackIdx, clickQN, p.laneIndex, pitch, false);
        return;
    }

    if (drums) return;

    enterSustainDrag(trackIdx, p.hitNoteStartQN, p.laneIndex, pitch, true);
}

void WriteController::handleUpdateSustain(const AuthoringPoint& p)
{
    double dragQN  = snapQN(p.rawProjectQN);

    if (dragQN - sustainDragStartQN < double(MIDI_MIN_SUSTAIN_LENGTH))
    {
        overlayState.drawPreviewVisible = false;
        overlayState.drawPreviewNotes.clear();
        return;
    }

    overlayState.drawPreviewVisible = true;
    overlayState.drawPreviewNotes.clear();
    overlayState.drawPreviewNotes.push_back({
        sustainDragLane, sustainDragStartQN, dragQN, sustainDragPitch
    });
}

void WriteController::handleCommitSustain(const AuthoringPoint& p)
{
    double endQN = snapQN(p.rawProjectQN);

    if (endQN - sustainDragStartQN >= double(MIDI_MIN_SUSTAIN_LENGTH))
    {
        if (sustainDragChainMode)
            chainExtendNotes(sustainDragTrackIdx, sustainDragStartQN, endQN, sustainDragPitch);
        else
            extendNote(sustainDragTrackIdx, sustainDragStartQN, endQN, sustainDragPitch);
    }

    endBatch();
    clearSustainDrag();
    recomputeGhost();
}

//==============================================================================
// Paint command handlers

void WriteController::handleBeginPaint(const AuthoringPoint& p, int trackIdx,
                                       [[maybe_unused]] int pitch, [[maybe_unused]] bool drums)
{
    double qn = snapQN(p.rawProjectQN);

    paintDragActive   = true;
    paintDragTrackIdx = trackIdx;
    paintStartQN      = qn;
    paintLastQN        = qn;
    paintLastLane      = p.laneIndex;
    paintedNotes.clear();
    beginBatch("Chartchotic: Paint notes");

    paintFillRange(qn, qn, p.laneIndex);
}

void WriteController::handleContinuePaint(const AuthoringPoint& p)
{
    if (!p.onHighway || p.laneIndex < 0) return;

    double cursorQN = snapQN(p.rawProjectQN);
    int lane = p.laneIndex;

    double lo = std::min(paintStartQN, cursorQN);
    double hi = std::max(paintStartQN, cursorQN);

    paintShrinkTo(lo, hi);
    paintFillRange(lo, hi, lane);

    paintLastQN   = cursorQN;
    paintLastLane = lane;
}

void WriteController::handleCommitPaint()
{
    endBatch();
    paintDragActive   = false;
    paintDragTrackIdx = -1;
    paintedNotes.clear();
    recomputeGhost();
}

void WriteController::paintFillRange(double fromQN, double toQN, int lane)
{
    bool drums = isDrums();
    int  pitch = resolvePitch(lane, drums);
    if (pitch < 0) return;

    double spacing = stepSpacingQN(currentStepDivision, currentTuplet);
    if (spacing <= 0.0) return;

    double lo = std::min(fromQN, toQN);
    double hi = std::max(fromQN, toQN);

    double firstStep = snapToStep(lo, currentStepDivision, currentTuplet);
    if (firstStep < lo - 0.001) firstStep += spacing;

    for (double qn = firstStep; qn <= hi + 0.001; qn += spacing)
    {
        double snapped = snapToStep(qn, currentStepDivision, currentTuplet);

        PaintedNote* existing = nullptr;
        for (auto& pn : paintedNotes)
            if (std::abs(pn.qn - snapped) < 0.001) { existing = &pn; break; }

        if (existing)
        {
            if (existing->lane != lane)
            {
                int oldPitch = resolvePitch(existing->lane, drums);
                if (oldPitch >= 0)
                    eraseNote(paintDragTrackIdx, existing->qn, oldPitch, drums, existing->lane, currentActiveSkill);
                createNote(paintDragTrackIdx, existing->qn, pitch, lane);
                existing->lane = lane;
            }
            continue;
        }

        auto pre = findNote(paintDragTrackIdx, snapped, pitch);
        if (pre.noteIndex >= 0 && std::abs(pre.startQN - snapped) < 0.001)
            continue;

        createNote(paintDragTrackIdx, snapped, pitch, lane);
        paintedNotes.push_back({ snapped, lane });
    }
}

void WriteController::paintShrinkTo(double lo, double hi)
{
    bool drums = isDrums();
    for (auto it = paintedNotes.begin(); it != paintedNotes.end(); )
    {
        if (it->qn < lo - 0.001 || it->qn > hi + 0.001)
        {
            int oldPitch = resolvePitch(it->lane, drums);
            if (oldPitch >= 0)
                eraseNote(paintDragTrackIdx, it->qn, oldPitch, drums, it->lane, currentActiveSkill);
            it = paintedNotes.erase(it);
        }
        else
            ++it;
    }
}

//==============================================================================
// Erase command handlers

void WriteController::handleBeginErase(const AuthoringPoint& p, int trackIdx, int pitch, bool drums)
{
    eraseDragActive   = true;
    eraseDragTrackIdx = trackIdx;
    beginBatch("Chartchotic: Erase notes");

    if (!p.overExistingNote) return;

    if (p.hitSustainBody)
        truncateNote(trackIdx, p.hitNoteStartQN, pitch);
    else
        eraseNote(trackIdx, p.hitNoteStartQN, pitch, drums, p.laneIndex, currentActiveSkill);
}

void WriteController::handleContinueErase(const AuthoringPoint& p)
{
    if (!p.onHighway || p.laneIndex < 0) return;
    if (!p.overExistingNote) return;

    bool drums = isDrums();
    int  pitch = resolvePitch(p.laneIndex, drums);
    if (pitch < 0) return;

    if (p.hitSustainBody)
        truncateNote(eraseDragTrackIdx, p.hitNoteStartQN, pitch);
    else
        eraseNote(eraseDragTrackIdx, p.hitNoteStartQN, pitch, drums, p.laneIndex, currentActiveSkill);
}

void WriteController::handleEndErase()
{
    endBatch();
    eraseDragActive   = false;
    eraseDragTrackIdx = -1;
}

//==============================================================================
// Key commands

bool WriteController::onKeyPress(const juce::KeyPress& key)
{
    auto cmd = commandMapper.resolveKey(writeModeActive(), key);

    switch (cmd)
    {
        case WriteCommand::ToggleWriteMode: setWriteModeActive(!writeModeActive()); return true;
        case WriteCommand::ToggleSubMode:   setSubMode(subMode() == SubMode::Draw ? SubMode::Edit : SubMode::Draw); return true;
        case WriteCommand::ToggleSnap:      setSnapEnabled(!snapEnabled());                   return true;
        case WriteCommand::StepDown:        setStepDivision(std::max(1, stepDivision() / 2)); return true;
        case WriteCommand::StepUp:          setStepDivision(stepDivision() * 2);              return true;
        case WriteCommand::CycleTuplet:     cycleTuplet();                                    return true;
        default: return false;
    }
}
