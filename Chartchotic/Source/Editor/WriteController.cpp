#include "WriteController.h"
#include "../Midi/Utils/MidiConstants.h"
#include <limits>

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
        && (barModeFlag || p.laneIndex >= 0)
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
    overlayState.ghostGem        = Gem::NOTE;
    overlayState.stampGhosts.clear();

    if (!lastPointValid)                                  return;
    if (!writeModeActive())                               return;
    if (isPlaying())                                      return;
    if (!lastPoint.onHighway || lastPoint.laneIndex < 0)  return;

    double qn = snapQN(lastPoint.rawProjectQN);
    if (qn < 0.0) qn = 0.0;

    overlayState.ghostVisible = true;
    overlayState.ghostQN      = qn;

    if (currentSubMode == SubMode::Draw
        && !sustainDragActive && !paintDragActive && !eraseDragActive
        && !stampCaptureActive)
    {
        if (!stamp.empty())
        {
            int minLane = stamp[0].lane, maxStampLane = stamp[0].lane;
            for (const auto& sn : stamp)
            {
                minLane = std::min(minLane, sn.lane);
                maxStampLane = std::max(maxStampLane, sn.lane);
            }
            stampMouseLaneOffset = juce::jlimit(-minLane, maxLane() - maxStampLane,
                                                lastPoint.laneIndex - minLane);
            for (const auto& sn : stamp)
                overlayState.stampGhosts.push_back({ sn.lane + stampMouseLaneOffset, sn.qnOffset, sn.duration });
        }
        else
        {
            overlayState.ghostLane = lastPoint.laneIndex;
            overlayState.ghostGem = resolveGhostGem(lastPoint.laneIndex);
        }
    }
}

void WriteController::setStamp(std::vector<StampNote> s)
{
    stamp = std::move(s);
    stateDidChange();
}

void WriteController::clearStamp()
{
    stamp.clear();
    stateDidChange();
}

void WriteController::shiftStampLanes(int delta)
{
    if (stamp.empty()) return;
    int maxLane = isDrums() ? 4 : 5;

    for (const auto& sn : stamp)
        if (sn.lane + delta < 0 || sn.lane + delta > maxLane) return;

    for (auto& sn : stamp)
        sn.lane += delta;

    stateDidChange();
}

void WriteController::beginStampCapture()
{
    if (stampCaptureActive) return;
    if (!lastPointValid || !lastPoint.onHighway || lastPoint.laneIndex < 0) return;

    int trackIdx = resolveTrackIdx();
    if (trackIdx < 0) return;

    stampCaptureActive   = true;
    stampCaptureTrackIdx = trackIdx;
    stampCaptureRect.begin(lastPoint.rawProjectQN, lastPoint.laneIndex);
    overlayState.marqueeVisible = true;
    overlayState.marqueeErase   = false;
    overlayState.marqueeRect    = stampCaptureRect;
    if (onStateChanged) onStateChanged();
}

void WriteController::enterSustainDrag(int trackIdx, double startQN, int lane, int pitch)
{
    sustainDragActive    = true;
    sustainDragTrackIdx  = trackIdx;
    sustainDragStartQN   = startQN;
    sustainDragLane      = lane;
    sustainDragPitch     = pitch;
}

void WriteController::clearSustainDrag()
{
    sustainDragActive    = false;
    sustainDragTrackIdx  = -1;
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

    if (stampCaptureActive && p.onHighway && p.laneIndex >= 0)
    {
        stampCaptureRect.update(p.rawProjectQN, p.laneIndex, barModeFlag, isDrums());
        overlayState.marqueeRect = stampCaptureRect;
        if (onStateChanged) onStateChanged();
    }

    recomputeGhost();
}

void WriteController::onPointerDown(const AuthoringPoint& p, const AuthoringContext& ctx)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    if (!canWrite(p)) return;

    int trackIdx = resolveTrackIdx();
    if (trackIdx < 0) return;

    bool drums = isDrums();
    int  pitch = resolveActivePitch(p.laneIndex);
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

    sustainPendingClick = false;
    if (sustainDragActive)   { handleUpdateSustain(p);  return; }
    if (paintDragActive)     { handleContinuePaint(p);  return; }
    if (eraseDragActive)     { handleContinueErase(p);  return; }
}

void WriteController::onPointerUp(const AuthoringPoint& p,
                                  [[maybe_unused]] const AuthoringContext& ctx)
{
    if (sustainPendingClick)
    {
        sustainPendingClick = false;
        createNote(sustainDragTrackIdx, sustainPendingClickQN,
                   sustainDragPitch, sustainDragLane, resolveVelocity());
        if (isDrums())
            writeTomMarker(sustainDragTrackIdx, sustainPendingClickQN, sustainDragLane);
        else
            writeGuitarForceMarker(sustainDragTrackIdx, sustainPendingClickQN);
        endBatch();
        clearSustainDrag();
        recomputeGhost();
        return;
    }
    if (sustainDragActive)   { handleCommitSustain(p);  return; }
    if (paintDragActive)     { handleCommitPaint();     return; }
    if (eraseDragActive)     { handleEndErase();        return; }
}

void WriteController::onFrameTick([[maybe_unused]] double currentProjectQN,
                                  [[maybe_unused]] bool isPlaying)
{
    if (stampCaptureActive && !juce::KeyPress::isKeyCurrentlyDown('C'))
    {
        stampCaptureActive = false;
        overlayState.marqueeVisible = false;

        auto classified = classifyNotesInRect(stampCaptureTrackIdx, stampCaptureRect);
        double minQN = std::numeric_limits<double>::max();
        for (const auto& cn : classified)
            if (!cn.sustainOnly && cn.note.startQN < minQN) minQN = cn.note.startQN;

        std::vector<StampNote> notes;
        for (const auto& cn : classified)
        {
            if (cn.sustainOnly) continue;
            notes.push_back({ cn.lane, cn.note.startQN - minQN,
                              cn.note.endQN - cn.note.startQN });
        }
        if (notes.size() >= 2)
            setStamp(std::move(notes));

        if (onStateChanged) onStateChanged();
    }

    recomputeGhost();
}

//==============================================================================
// Sustain command handlers

void WriteController::handleBeginSustain(const AuthoringPoint& p, int trackIdx, int pitch, bool drums)
{
    double clickQN = snapQN(p.rawProjectQN);

    if (!stamp.empty())
    {
        beginBatch("Chartchotic: Stamp notes");
        for (const auto& sn : stamp)
        {
            int lane = sn.lane + stampMouseLaneOffset;
            int sp = resolvePitch(lane, drums);
            if (sp >= 0)
            {
                createNote(trackIdx, clickQN + sn.qnOffset, sp, lane, resolveVelocity(), sn.duration);
                if (drums)
                    writeTomMarker(trackIdx, clickQN + sn.qnOffset, lane);
                else
                    writeGuitarForceMarker(trackIdx, clickQN + sn.qnOffset);
            }
        }
        if (drums) { endBatch(); return; }
        enterSustainDrag(trackIdx, clickQN, p.laneIndex, pitch);
        return;
    }

    bool onExistingNote = p.overExistingNote
        && findNote(trackIdx, p.hitNoteStartQN, pitch).noteIndex >= 0;

    if (drums)
    {
        if (!onExistingNote)
        {
            createNote(trackIdx, clickQN, pitch, p.laneIndex, resolveVelocity());
            writeTomMarker(trackIdx, clickQN, p.laneIndex);
        }
        return;
    }

    beginBatch("Chartchotic: Sustain note");

    if (!onExistingNote)
    {
        createNote(trackIdx, clickQN, pitch, p.laneIndex, resolveVelocity());
        writeGuitarForceMarker(trackIdx, clickQN);
        enterSustainDrag(trackIdx, clickQN, p.laneIndex, pitch);
        return;
    }

    enterSustainDrag(trackIdx, p.hitNoteStartQN, p.laneIndex, pitch);
    sustainPendingClick = true;
    sustainPendingClickQN = clickQN;
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
    if (!stamp.empty())
    {
        bool drums = isDrums();
        for (const auto& sn : stamp)
        {
            int lane = sn.lane + stampMouseLaneOffset;
            overlayState.drawPreviewNotes.push_back({
                lane, sustainDragStartQN + sn.qnOffset, dragQN, resolvePitch(lane, drums)
            });
        }
    }
    else
    {
        overlayState.drawPreviewNotes.push_back({
            sustainDragLane, sustainDragStartQN, dragQN, sustainDragPitch
        });
    }
}

void WriteController::handleCommitSustain(const AuthoringPoint& p)
{
    double endQN = snapQN(p.rawProjectQN);

    if (endQN - sustainDragStartQN >= double(MIDI_MIN_SUSTAIN_LENGTH))
    {
        if (!stamp.empty())
        {
            bool drums = isDrums();
            for (const auto& sn : stamp)
            {
                int lane = sn.lane + stampMouseLaneOffset;
                int sp = resolvePitch(lane, drums);
                if (sp >= 0)
                    chainExtendNotes(sustainDragTrackIdx,
                                     sustainDragStartQN + sn.qnOffset,
                                     endQN, sp);
            }
        }
        else
        {
            chainExtendNotes(sustainDragTrackIdx, sustainDragStartQN, endQN, sustainDragPitch);
        }
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
    int lane = paintLastLane;

    if (p.laneIndex != paintLastLane)
    {
        paintShrinkTo(-1.0, -1.0);
        paintLastQN   = cursorQN;
        paintLastLane = p.laneIndex;
        return;
    }

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

    double spacing = stepSpacingQN(currentStepDivision, currentTuplet);
    if (spacing <= 0.0) return;

    double lo = std::min(fromQN, toQN);
    double hi = std::max(fromQN, toQN);

    double firstStep = snapToStep(lo, currentStepDivision, currentTuplet);
    if (firstStep < lo - 0.001) firstStep += spacing;

    for (double qn = firstStep; qn <= hi + 0.001; qn += spacing)
    {
        double snapped = snapToStep(qn, currentStepDivision, currentTuplet);

        bool alreadyPainted = false;
        for (const auto& pn : paintedNotes)
            if (std::abs(pn.qn - snapped) < 0.001) { alreadyPainted = true; break; }
        if (alreadyPainted) continue;

        if (!stamp.empty())
        {
            for (const auto& sn : stamp)
            {
                int lane = sn.lane + stampMouseLaneOffset;
                int sp = resolvePitch(lane, drums);
                if (sp >= 0)
                {
                    createNote(paintDragTrackIdx, snapped + sn.qnOffset, sp, lane, resolveVelocity());
                    if (drums)
                        writeTomMarker(paintDragTrackIdx, snapped + sn.qnOffset, lane);
                    else
                        writeGuitarForceMarker(paintDragTrackIdx, snapped + sn.qnOffset);
                }
            }
        }
        else
        {
            int pitch = resolveActivePitch(lane);
            if (pitch < 0) continue;
            auto pre = findNote(paintDragTrackIdx, snapped, pitch);
            if (pre.noteIndex >= 0 && std::abs(pre.startQN - snapped) < 0.001)
                continue;
            createNote(paintDragTrackIdx, snapped, pitch, lane, resolveVelocity());
            if (drums)
                writeTomMarker(paintDragTrackIdx, snapped, lane);
            else
                writeGuitarForceMarker(paintDragTrackIdx, snapped);
        }
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
            if (!stamp.empty())
            {
                for (const auto& sn : stamp)
                {
                    int lane = sn.lane + stampMouseLaneOffset;
                    int sp = resolvePitch(lane, drums);
                    if (sp >= 0)
                        eraseNote(paintDragTrackIdx, it->qn + sn.qnOffset, sp, drums, lane, currentActiveSkill);
                }
            }
            else
            {
                int oldPitch = resolveActivePitch(it->lane);
                if (oldPitch >= 0)
                    eraseNote(paintDragTrackIdx, it->qn, oldPitch, drums, it->lane, currentActiveSkill);
            }
            it = paintedNotes.erase(it);
        }
        else
            ++it;
    }
}

//==============================================================================
// Erase command handlers

void WriteController::handleBeginErase(const AuthoringPoint& p, int trackIdx,
                                       [[maybe_unused]] int pitch, [[maybe_unused]] bool drums)
{
    if (!stamp.empty()) clearStamp();
    eraseDragActive   = true;
    eraseDragTrackIdx = trackIdx;
    eraseRect.begin(p.rawProjectQN, p.laneIndex);
    eraseClickedNoteQN = (p.overExistingNote && !p.hitSustainBody) ? p.hitNoteStartQN : -1.0;
    eraseClickedSustainQN   = (p.overExistingNote && p.hitSustainBody) ? p.hitNoteStartQN : -1.0;
    eraseClickedSustainLane = (p.overExistingNote && p.hitSustainBody) ? p.laneIndex : -1;

    overlayState.marqueeVisible = true;
    overlayState.marqueeErase   = true;
    overlayState.marqueeRect    = eraseRect;
    overlayState.eraseClickedNoteQN     = eraseClickedNoteQN;
    overlayState.eraseClickedLane       = eraseClickedNoteQN >= 0.0 ? p.laneIndex : -1;
    overlayState.eraseClickedSustainQN  = eraseClickedSustainQN;
    overlayState.eraseClickedSustainLane = eraseClickedSustainLane;
    if (onStateChanged) onStateChanged();
}

void WriteController::handleContinueErase(const AuthoringPoint& p)
{
    if (!p.onHighway || p.laneIndex < 0) return;

    eraseRect.update(p.rawProjectQN, p.laneIndex, barModeFlag, isDrums());

    overlayState.marqueeVisible = true;
    overlayState.marqueeErase   = true;
    overlayState.marqueeRect    = eraseRect;
    if (onStateChanged) onStateChanged();
}

void WriteController::handleEndErase()
{
    bool drums = isDrums();
    beginBatch("Chartchotic: Erase notes");
    for (const auto& cn : classifyNotesInRect(eraseDragTrackIdx, eraseRect))
    {
        if (cn.sustainOnly)
            truncateNote(eraseDragTrackIdx, cn.note.startQN, cn.note.pitch);
        else
            eraseNote(eraseDragTrackIdx, cn.note.startQN, cn.note.pitch,
                      drums, cn.lane, currentActiveSkill);
    }
    if (eraseClickedNoteQN >= 0.0)
    {
        if (barModeFlag && drums)
        {
            for (int tryLane : {0, 6})
            {
                int tryPitch = resolveBarPitch(tryLane);
                if (tryPitch >= 0 && findNote(eraseDragTrackIdx, eraseClickedNoteQN, tryPitch).noteIndex >= 0)
                {
                    eraseNote(eraseDragTrackIdx, eraseClickedNoteQN, tryPitch, true, tryLane, currentActiveSkill);
                    break;
                }
            }
        }
        else
        {
            int pitch = resolveActivePitch(eraseRect.startLane);
            if (pitch >= 0)
                eraseNote(eraseDragTrackIdx, eraseClickedNoteQN, pitch, drums,
                          eraseRect.startLane, currentActiveSkill);
        }
    }
    if (eraseClickedSustainQN >= 0.0)
    {
        int pitch = resolveActivePitch(eraseClickedSustainLane);
        if (pitch >= 0)
            truncateNote(eraseDragTrackIdx, eraseClickedSustainQN, pitch);
    }
    endBatch();

    eraseDragActive   = false;
    eraseDragTrackIdx = -1;
    eraseClickedNoteQN      = -1.0;
    eraseClickedSustainQN   = -1.0;
    eraseClickedSustainLane = -1;
    overlayState.marqueeVisible         = false;
    overlayState.marqueeErase           = false;
    overlayState.eraseClickedNoteQN     = -1.0;
    overlayState.eraseClickedLane       = -1;
    overlayState.eraseClickedSustainQN  = -1.0;
    overlayState.eraseClickedSustainLane = -1;
    recomputeGhost();
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
