#include "WriteController.h"

#include <cmath>

#include "../Midi/Providers/MidiWriter.h"
#include "../Midi/InstrumentSession.h"
#include "../Midi/Discovery/TrackDiscovery.h"
#include "../Midi/Utils/InstrumentMapper.h"

namespace
{
    // Persisted ValueTree property keys
    const juce::Identifier kWriteSubMode      { "writeSubMode" };       // "draw" / "edit"
    const juce::Identifier kWriteStepDivision { "writeStepDivision" };  // int 1..64
    const juce::Identifier kWriteTuplet       { "writeTuplet" };        // int 0/3/5/7
    const juce::Identifier kWriteSnap         { "writeSnap" };          // bool

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

    // writeModeActive is intentionally NOT loaded — always starts off per the plan.
}

//==============================================================================
// Setters

void WriteController::setWriteModeActive(bool active)
{
    if (writeModeActiveFlag == active) return;
    writeModeActiveFlag = active;
    recomputeGhost();
    if (onStateChanged) onStateChanged();
}

void WriteController::setSubMode(SubMode mode)
{
    if (currentSubMode == mode) return;
    currentSubMode = mode;
    state.setProperty(kWriteSubMode, subModeToString(mode), nullptr);
    recomputeGhost();
    if (onStateChanged) onStateChanged();
}

void WriteController::setStepDivision(int division)
{
    int clamped = juce::jlimit(1, 64, division);
    if (currentStepDivision == clamped) return;
    currentStepDivision = clamped;
    state.setProperty(kWriteStepDivision, clamped, nullptr);
    recomputeGhost();
    if (onStateChanged) onStateChanged();
}

void WriteController::setTuplet(int t)
{
    if (t != 0 && t != 3 && t != 5 && t != 7) return;
    if (currentTuplet == t) return;
    currentTuplet = t;
    state.setProperty(kWriteTuplet, t, nullptr);
    recomputeGhost();
    if (onStateChanged) onStateChanged();
}

void WriteController::setSnapEnabled(bool enabled)
{
    if (snapEnabledFlag == enabled) return;
    snapEnabledFlag = enabled;
    state.setProperty(kWriteSnap, enabled, nullptr);
    recomputeGhost();
    if (onStateChanged) onStateChanged();
}

void WriteController::setActivePart(Part part)
{
    currentActivePart = part;
}

void WriteController::setActiveSkill(SkillLevel skill)
{
    currentActiveSkill = skill;
}

//==============================================================================
// Helpers (file-scope, unit-testable in isolation)

namespace
{
    // Fixed short-note duration in QN for click-to-place. Drag-to-sustain
    // will replace this in a future milestone.
    constexpr double kShortNoteDurationQN = 0.1;

    // IMPORTANT: must stay in lock-step with GridlineGenerator.h's stepSpacingQN
    // formula (search there for the same expression). The snapped position must
    // land exactly on a rendered gridline; if these diverge, clicks land off-grid.
    double stepSpacingQN(int stepDivision, int tuplet)
    {
        if (tuplet > 0)
            return 8.0 / (double(stepDivision) * double(tuplet));
        return 4.0 / double(stepDivision);
    }

    double snapToStep(double rawQN, int stepDivision, int tuplet)
    {
        const double spacing = stepSpacingQN(stepDivision, tuplet);
        if (spacing <= 0.0)
            return rawQN;

        double snapped = std::round(rawQN / spacing) * spacing;

        // Final precision floor: round to nearest 1/128 QN if the residual is
        // below ~1e-6 (kills floating-point fuzz that would otherwise leak into
        // REAPER's PPQ conversion).
        const double oneOver128 = 1.0 / 128.0;
        const double quantized  = std::round(snapped / oneOver128) * oneOver128;
        if (std::abs(snapped - quantized) < 1e-6)
            snapped = quantized;
        return snapped;
    }

    // Find the first track in the session whose .part matches `part`. Returns
    // the REAPER (backend-opaque) track index, or -1 if not found.
    int resolveTrackIndexForPart(InstrumentSession* session, Part part)
    {
        if (session == nullptr) return -1;
        for (const auto& info : session->getTracks())
            if (info.part == part)
                return info.sourceTrackIndex;
        return -1;
    }
}

//==============================================================================
// Pointer / key / frame input methods. onPointerDown handles left-click
// placement; the rest are stubs until later milestones.

void WriteController::onPointerMove(const AuthoringPoint& p,
                                    [[maybe_unused]] const AuthoringContext& ctx)
{
    JUCE_ASSERT_MESSAGE_THREAD;
    lastPoint = p;
    lastPointValid = true;
    recomputeGhost();
}

void WriteController::recomputeGhost()
{
    overlayState.ghostVisible    = false;
    overlayState.ghostLane       = -1;
    overlayState.ghostQN         = 0.0;
    overlayState.ghostShowsErase = false;

    if (!lastPointValid)                          return;
    if (!writeModeActive())                       return;
    if (playingStatePtr && *playingStatePtr)       return;
    if (currentSubMode != SubMode::Draw)          return;
    if (!lastPoint.onHighway || lastPoint.laneIndex < 0) return;

    double qn = snapEnabled()
        ? snapToStep(lastPoint.rawProjectQN, currentStepDivision, currentTuplet)
        : lastPoint.rawProjectQN;

    if (qn < 0.0) qn = 0.0;

    overlayState.ghostVisible = true;
    overlayState.ghostLane    = lastPoint.laneIndex;
    overlayState.ghostQN      = qn;
}

void WriteController::onPointerDown(const AuthoringPoint& p, const AuthoringContext& ctx)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    // Left-click in Draw places a single short note. Everything else
    // (right-click erase, drag-to-sustain, hover ghost, hit-test for "click on
    // existing = no-op") is deferred to later milestones.
    if (!writeModeActive())                  return;
    if (playingStatePtr && *playingStatePtr)  return;
    if (currentSubMode != SubMode::Draw)     return;
    if (!ctx.leftButton)                     return;
    if (!p.onHighway)                        return;
    if (p.laneIndex < 0)                     return;
    if (midiWriter == nullptr)               return;
    if (!midiWriter->isAvailable())          return;
    if (instrumentSession == nullptr)        return;

    const int trackIdx = resolveTrackIndexForPart(instrumentSession, currentActivePart);
    if (trackIdx < 0) return;

    const bool drums = isDrumLike(currentActivePart);
    const int  pitch = drums
        ? InstrumentMapper::columnToDrumPitch  (currentActiveSkill, p.laneIndex, /*kick2x*/ false)
        : InstrumentMapper::columnToGuitarPitch(currentActiveSkill, p.laneIndex);
    if (pitch < 0) return;

    const double startQN = snapEnabled()
        ? snapToStep(p.rawProjectQN, currentStepDivision, currentTuplet)
        : p.rawProjectQN;

    // Visually reads as a tap gem in any sane step grid, large enough that
    // REAPER doesn't drop it.
    const double endQN = startQN + kShortNoteDurationQN;

    if (midiWriter->insertNote(trackIdx, startQN, endQN, /*channel*/ 0, pitch, /*velocity*/ 100))
        instrumentSession->invalidateTrack(trackIdx);
}

void WriteController::onPointerDrag([[maybe_unused]] const AuthoringPoint& p,
                                    [[maybe_unused]] const AuthoringContext& ctx) {}

void WriteController::onPointerUp([[maybe_unused]] const AuthoringPoint& p,
                                  [[maybe_unused]] const AuthoringContext& ctx) {}

void WriteController::onPointerExit()
{
    lastPointValid = false;
    overlayState.ghostVisible   = false;
    overlayState.ghostLane      = -1;
    overlayState.ghostQN        = 0.0;
    overlayState.ghostShowsErase = false;
}

void WriteController::onPointerCancel() {}

bool WriteController::onKeyPress(const juce::KeyPress& key)
{
    const int code = key.getKeyCode();

    // W toggles write mode regardless of current state.
    if (code == 'W')
    {
        setWriteModeActive(!writeModeActive());
        return true;
    }

    // Remaining shortcuts only apply while write mode is active.
    if (!writeModeActive())
        return false;

    if (code == 'Q')
    {
        setSubMode(subMode() == SubMode::Draw ? SubMode::Edit : SubMode::Draw);
        return true;
    }

    if (code == 'S')
    {
        setSnapEnabled(!snapEnabled());
        return true;
    }

    if (code == 'T')
    {
        // Cycle 0 -> 3 -> 5 -> 7 -> 0
        int next = 0;
        switch (tuplet())
        {
            case 0: next = 3; break;
            case 3: next = 5; break;
            case 5: next = 7; break;
            case 7: next = 0; break;
            default: next = 0; break;
        }
        setTuplet(next);
        return true;
    }

    if (code == '[')
    {
        // Halve, clamped to min 1 (setter no-ops if same value).
        setStepDivision(std::max(1, stepDivision() / 2));
        return true;
    }

    if (code == ']')
    {
        // Double; setter clamps to max 64.
        setStepDivision(stepDivision() * 2);
        return true;
    }

    return false;
}

void WriteController::onFrameTick([[maybe_unused]] double currentProjectQN,
                                  [[maybe_unused]] bool isPlaying)
{
    recomputeGhost();
}
