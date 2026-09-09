#include "WriteController.h"

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
    writeModeActiveFlag = active;
}

void WriteController::setSubMode(SubMode mode)
{
    if (currentSubMode == mode) return;
    currentSubMode = mode;
    state.setProperty(kWriteSubMode, subModeToString(mode), nullptr);
}

void WriteController::setStepDivision(int division)
{
    int clamped = juce::jlimit(1, 64, division);
    if (currentStepDivision == clamped) return;
    currentStepDivision = clamped;
    state.setProperty(kWriteStepDivision, clamped, nullptr);
}

void WriteController::setTuplet(int t)
{
    if (t != 0 && t != 3 && t != 5 && t != 7) return;
    if (currentTuplet == t) return;
    currentTuplet = t;
    state.setProperty(kWriteTuplet, t, nullptr);
}

void WriteController::setSnapEnabled(bool enabled)
{
    if (snapEnabledFlag == enabled) return;
    snapEnabledFlag = enabled;
    state.setProperty(kWriteSnap, enabled, nullptr);
}

void WriteController::setActivePart(Part part)
{
    currentActivePart = part;
}

//==============================================================================
// Input methods — no-ops in M1.1.

void WriteController::onPointerMove([[maybe_unused]] const AuthoringPoint& p,
                                    [[maybe_unused]] const AuthoringContext& ctx) {}

void WriteController::onPointerDown([[maybe_unused]] const AuthoringPoint& p,
                                    [[maybe_unused]] const AuthoringContext& ctx) {}

void WriteController::onPointerDrag([[maybe_unused]] const AuthoringPoint& p,
                                    [[maybe_unused]] const AuthoringContext& ctx) {}

void WriteController::onPointerUp([[maybe_unused]] const AuthoringPoint& p,
                                  [[maybe_unused]] const AuthoringContext& ctx) {}

void WriteController::onPointerExit() {}

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
                                  [[maybe_unused]] bool isPlaying) {}
