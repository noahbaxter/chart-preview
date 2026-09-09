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
    overlayState.ghostModeLabel  = {};
    overlayState.stampGhosts.clear();

    if (!lastPointValid)                                  return;
    if (!writeModeActive())                               return;
    if (isPlaying())                                      return;
    if (!lastPoint.onHighway || lastPoint.laneIndex < 0)  return;

    double qn = snapQN(lastPoint.rawProjectQN);
    if (qn < 0.0) qn = 0.0;

    overlayState.ghostVisible = true;
    overlayState.ghostQN      = qn;

    // Outside the drag guard below: the mode is still in force while you are
    // painting, so the hint stays up for as long as shift is held.
    if (altKickArmed)
        overlayState.ghostModeLabel = "ALTERNATE KICKS";

    if (currentSubMode == SubMode::Draw
        && !sustainDragActive && !paintDragActive && !eraseDragActive
        && !stampCaptureActive)
    {
        if (!stamp.empty())
        {
            // The stamp holds the lanes it was copied from. The mouse moves it
            // in time only; left/right arrows are the only way to move it
            // across lanes, via shiftStampLanes.
            for (const auto& sn : stamp)
                overlayState.stampGhosts.push_back({ sn.lane, sn.qnOffset, sn.duration, sn.gem });
        }
        else if (altKickArmed)
        {
            // A shift-drag from here lays an alternating roll that always opens
            // on 1x, so preview the 1x lane rather than whichever side the
            // mouse is on.
            overlayState.ghostLane = DRUM_KICK_COLUMN;
            overlayState.ghostGem  = resolveGhostGem(DRUM_KICK_COLUMN);
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

    // maxLane() rather than a local count: arrows are now the only way to move
    // a stamp sideways, so they have to reach every lane the mouse used to,
    // kick lanes included when kick2x is on.
    for (const auto& sn : stamp)
        if (sn.lane + delta < 0 || sn.lane + delta > maxLane()) return;

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

void WriteController::toggleTuplet()
{
    if (currentTuplet != 0)
    {
        lastTuplet = currentTuplet;
        setTuplet(0);
        return;
    }
    setTuplet(lastTuplet);
}

void WriteController::cycleTuplet()
{
    // Always lands on a live tuplet, so this doubles as "turn it on" when off.
    switch (currentTuplet)
    {
        case 3:  lastTuplet = 5; break;
        case 5:  lastTuplet = 7; break;
        default: lastTuplet = 3; break;
    }
    setTuplet(lastTuplet);
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
        // Replacing a note's type must not destroy its sustain, so carry the
        // existing duration across when we land on the same note.
        auto existing = findNote(sustainDragTrackIdx, sustainPendingClickQN, sustainDragPitch);
        double duration = (existing.noteIndex >= 0
                           && std::abs(existing.startQN - sustainPendingClickQN) < kQNEpsilon)
                        ? existing.endQN - existing.startQN : 0.0;
        placeNote(sustainDragTrackIdx, sustainPendingClickQN,
                  sustainDragPitch, sustainDragLane, resolveVelocity(), duration);
        endBatch();
        clearSustainDrag();
        recomputeGhost();
        return;
    }
    if (sustainDragActive)   { handleCommitSustain(p);  return; }
    if (paintDragActive)     { handleCommitPaint();     return; }
    if (eraseDragActive)     { handleEndErase();        return; }
}

bool WriteController::altKickAvailable() const
{
    return writeModeActive()
        && currentSubMode == SubMode::Draw
        && barModeFlag
        && isDrums()
        && kick2xEnabled;
}

void WriteController::onFrameTick([[maybe_unused]] double currentProjectQN,
                                  [[maybe_unused]] bool isPlaying)
{
    bool armed = altKickAvailable()
              && juce::ModifierKeys::getCurrentModifiers().isShiftDown();
    if (armed != altKickArmed)
    {
        altKickArmed = armed;
        recomputeGhost();
    }

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
            uint32_t mask = captureMarkerMask(stampCaptureTrackIdx, cn.note.startQN, cn.lane);
            notes.push_back({ cn.lane, cn.note.startQN - minQN,
                              cn.note.endQN - cn.note.startQN,
                              cn.note.velocity, mask,
                              resolveCapturedGem(cn.lane, cn.note.velocity, mask) });
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
            int lane = sn.lane;
            int sp = resolvePitch(lane, drums);
            if (sp >= 0)
            {
                // Velocity and markers come from the captured note, not the
                // current toolbar state, so a paste round-trips drum dynamics
                // and note type exactly as copied. One path for both
                // instruments: the mask says which markers to reproduce.
                placeStampNote(trackIdx, clickQN + sn.qnOffset, sp, lane,
                               sn.velocity, sn.duration, sn.markerMask);
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
        // No guard on an existing note: clicking one re-applies the current
        // type, so dropping an accent on a normal note upgrades it instead of
        // silently doing nothing. createNote erases and recreates at the same
        // QN, which makes this a replace. Targeting clickQN rather than the
        // hit note also means clicking further along a sustain body creates a
        // new note at that step, which previously fell through and did nothing.
        double duration = 0.0;
        if (onExistingNote && std::abs(p.hitNoteStartQN - clickQN) < kQNEpsilon)
        {
            auto found = findNote(trackIdx, p.hitNoteStartQN, pitch);
            if (found.noteIndex >= 0) duration = found.endQN - found.startQN;
        }
        placeNote(trackIdx, clickQN, pitch, p.laneIndex, resolveVelocity(), duration);
        return;
    }

    beginBatch("Chartchotic: Sustain note");

    if (!onExistingNote)
    {
        placeNote(trackIdx, clickQN, pitch, p.laneIndex, resolveVelocity());
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
            int lane = sn.lane;
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
                int lane = sn.lane;
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

    // Paint is only ever reached by shift-dragging, so kick mode needs no
    // extra modifier to mean "alternate": being here is the request.
    bool alternatingKicks = drums && barModeFlag && kick2xEnabled;

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
                int lane = sn.lane;
                int sp = resolvePitch(lane, drums);
                if (sp >= 0)
                {
                    // Painting a stamp is still a paste, so it carries the
                    // captured velocity and markers like the click path does.
                    placeStampNote(paintDragTrackIdx, snapped + sn.qnOffset, sp, lane,
                                   sn.velocity, 0.0, sn.markerMask);
                }
            }
        }
        else
        {
            // Painting kicks lays down an alternating 1x/2x roll rather than a
            // run of one pitch, so a 1x-only player still gets a playable
            // half-speed version of the pattern. Parity is anchored to
            // paintStartQN so it stays put when the drag reverses or refills,
            // and the run always opens on 1x.
            int paintLane = lane;
            if (alternatingKicks)
            {
                long long step = std::llround((snapped - paintStartQN) / spacing);
                bool offbeat = ((step % 2) + 2) % 2 == 1;
                paintLane = offbeat ? DRUM_KICK_2X_COLUMN : DRUM_KICK_COLUMN;
            }

            int pitch = alternatingKicks
                ? InstrumentMapper::resolveKickPitch(currentActiveSkill, paintLane, kick2xEnabled)
                : resolveActivePitch(lane);
            if (pitch < 0) continue;
            auto pre = findNote(paintDragTrackIdx, snapped, pitch);
            if (pre.noteIndex >= 0 && std::abs(pre.startQN - snapped) < 0.001)
                continue;
            placeNote(paintDragTrackIdx, snapped, pitch, paintLane, resolveVelocity());
            paintedNotes.push_back({ snapped, paintLane });
            continue;
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
                    int lane = sn.lane;
                    int sp = resolvePitch(lane, drums);
                    if (sp >= 0)
                        eraseNote(paintDragTrackIdx, it->qn + sn.qnOffset, sp, lane, currentActiveSkill);
                }
            }
            else
            {
                int oldPitch = resolveActivePitch(it->lane);
                if (oldPitch >= 0)
                    eraseNote(paintDragTrackIdx, it->qn, oldPitch, it->lane, currentActiveSkill);
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
        {
            // A predecessor trimmed to butt against the note being clicked ends
            // exactly where the click is, so the raw cursor QN can land a hair
            // inside it. Erasing a head is not a reason to shorten its
            // neighbour.
            if (eraseClickedNoteQN >= 0.0
                && std::abs(cn.note.endQN - eraseClickedNoteQN) < kQNEpsilon)
                continue;
            truncateNote(eraseDragTrackIdx, cn.note.startQN, cn.note.pitch);
        }
        else
            eraseNote(eraseDragTrackIdx, cn.note.startQN, cn.note.pitch,
                      cn.lane, currentActiveSkill);
    }
    if (eraseClickedNoteQN >= 0.0)
    {
        if (barModeFlag && drums)
        {
            // Bar lanes are kick + 2x kick, and the 2x column differs per part (4-lane 6,
            // elite 9), so it can't be a literal pair.
            const int kick2xLane = isElite() ? ELITE_KICK_2X_COLUMN : DRUM_KICK_2X_COLUMN;
            for (int tryLane : {DRUM_KICK_COLUMN, kick2xLane})
            {
                int tryPitch = resolveBarPitch(tryLane);
                if (tryPitch >= 0 && findNote(eraseDragTrackIdx, eraseClickedNoteQN, tryPitch).noteIndex >= 0)
                {
                    eraseNote(eraseDragTrackIdx, eraseClickedNoteQN, tryPitch, tryLane, currentActiveSkill);
                    break;
                }
            }
        }
        else
        {
            int pitch = resolveActivePitch(eraseRect.startLane);
            if (pitch >= 0)
                eraseNote(eraseDragTrackIdx, eraseClickedNoteQN, pitch,
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
        case WriteCommand::ToggleTuplet:    toggleTuplet();                                   return true;
        case WriteCommand::CycleTuplet:     cycleTuplet();                                    return true;
        default: return false;
    }
}
