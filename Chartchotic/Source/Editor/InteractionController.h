#pragma once

#include <JuceHeader.h>
#include "WriteController.h"
#include "EditController.h"

class InteractionController
{
public:
    explicit InteractionController(juce::ValueTree& state);

    void onFrame(MidiWriter* writer, InstrumentSession* session,
                 const bool* playingState, Part activePart, SkillLevel activeSkill,
                 double currentProjectQN);

    void onPointerMove       (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerDown       (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerDrag       (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerUp         (const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerDoubleClick(const AuthoringPoint& p, const AuthoringContext& ctx);
    void onPointerExit();
    void onPointerCancel();
    bool onKeyPress(const juce::KeyPress& key);

    bool       writeModeActive() const { return writeController.writeModeActive(); }
    SubMode    subMode()         const { return writeController.subMode(); }
    int        stepDivision()    const { return writeController.stepDivision(); }
    int        tuplet()          const { return writeController.tuplet(); }
    bool       snapEnabled()     const { return writeController.snapEnabled(); }
    Part       activePart()      const { return writeController.activePart(); }
    SkillLevel activeSkill()     const { return writeController.activeSkill(); }

    void setWriteModeActive(bool v) { writeController.setWriteModeActive(v); }
    void setSubMode(SubMode m)      { writeController.setSubMode(m); }
    void setStepDivision(int d)     { writeController.setStepDivision(d); }
    void setTuplet(int t)           { writeController.setTuplet(t); }
    void cycleTuplet()              { writeController.cycleTuplet(); }
    void setSnapEnabled(bool v)     { writeController.setSnapEnabled(v); }
    void setActivePart(Part p)      { writeController.setActivePart(p); }
    void setActiveSkill(SkillLevel s){ writeController.setActiveSkill(s); }

    const OverlayState& getOverlayState() const;

    std::function<void()> onStateChanged;

private:
    bool isEditActive() const;

    WriteController writeController;
    EditController  editController;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InteractionController)
};
