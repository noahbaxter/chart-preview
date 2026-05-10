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

    bool        writeModeActive() const { return writeController.writeModeActive(); }
    SubMode     subMode()         const { return writeController.subMode(); }
    int         stepDivision()    const { return writeController.stepDivision(); }
    int         tuplet()          const { return writeController.tuplet(); }
    bool        snapEnabled()     const { return writeController.snapEnabled(); }
    bool        barMode()         const { return barModeFlag; }
    Part        activePart()      const { return writeController.activePart(); }
    SkillLevel  activeSkill()     const { return writeController.activeSkill(); }
    DrumDynamic drumDynamic()     const { return writeController.drumDynamic(); }
    GuitarForce guitarForce()     const { return writeController.guitarForce(); }
    bool        cymbalMode()      const { return writeController.cymbalMode(); }

    void setWriteModeActive(bool v) { writeController.setWriteModeActive(v); }
    void setSubMode(SubMode m)      { writeController.setSubMode(m); }
    void setStepDivision(int d)     { writeController.setStepDivision(d); }
    void setTuplet(int t)           { writeController.setTuplet(t); }
    void cycleTuplet()              { writeController.cycleTuplet(); }
    void setSnapEnabled(bool v)     { writeController.setSnapEnabled(v); }
    void clearEditSelection()       { editController.clearSelection(); }
    void setBarMode(bool v);
    void setActivePart(Part p)        { writeController.setActivePart(p); editController.setActivePart(p); }
    void setActiveSkill(SkillLevel s) { writeController.setActiveSkill(s); editController.setActiveSkill(s); }
    void setDrumDynamic(DrumDynamic d){ writeController.setDrumDynamic(d); editController.setDrumDynamic(d); }
    void setGuitarForce(GuitarForce f){ writeController.setGuitarForce(f); editController.setGuitarForce(f); }
    void setCymbalMode(bool c)        { writeController.setCymbalMode(c);  editController.setCymbalMode(c); }

    const OverlayState& getOverlayState() const;
    const OptimisticPatchBuffer& getPatchBuffer() const { return patchBuffer; }

    std::function<void()> onStateChanged;

private:
    bool isEditActive() const;

    juce::ValueTree& state;
    bool barModeFlag = false;
    OptimisticPatchBuffer patchBuffer;
    WriteController writeController;
    EditController  editController;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InteractionController)
};
