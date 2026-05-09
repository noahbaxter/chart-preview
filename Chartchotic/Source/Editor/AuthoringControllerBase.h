#pragma once

#include <JuceHeader.h>
#include "AuthoringTypes.h"
#include "AuthoringUtils.h"
#include "CommandMapper.h"
#include "NoteEditor.h"
#include "../UI/ControlConstants.h"
#include "../Midi/InstrumentSession.h"
#include "../Midi/Utils/InstrumentMapper.h"

class AuthoringControllerBase
{
public:
    void setMidiWriter(MidiWriter* w)              { noteEditor.setMidiWriter(w); }
    void setInstrumentSession(InstrumentSession* s){ instrumentSession = s; noteEditor.setInstrumentSession(s); }
    void setPlayingStatePtr(const bool* p)         { playingStatePtr = p; }
    void setPatchBuffer(OptimisticPatchBuffer* b)   { patchBuffer = b; }
    void setActivePart(Part p)                     { currentActivePart = p; }
    void setActiveSkill(SkillLevel s)              { currentActiveSkill = s; }
    void setStepDivision(int d)                    { currentStepDivision = d; }
    void setTuplet(int t)                          { currentTuplet = t; }
    void setSnapEnabled(bool s)                    { snapEnabledFlag = s; }
    void setBarMode(bool b)                        { barModeFlag = b; overlayState.barMode = b; }
    void setKick2x(bool k)                         { kick2xEnabled = k; }

    Part       activePart()    const { return currentActivePart; }
    SkillLevel activeSkill()   const { return currentActiveSkill; }
    int        stepDivision()  const { return currentStepDivision; }
    int        tuplet()        const { return currentTuplet; }
    bool       snapEnabled()   const { return snapEnabledFlag; }

    const OverlayState& getOverlayState() const { return overlayState; }

    std::function<void()> onStateChanged;

protected:
    bool isPlaying() const { return playingStatePtr && *playingStatePtr; }
    bool isDrums()   const { return isDrumLike(currentActivePart); }

    int resolvePitch(int laneIndex, bool drums) const
    {
        return drums
            ? InstrumentMapper::columnToDrumPitch(currentActiveSkill, laneIndex, false)
            : InstrumentMapper::columnToGuitarPitch(currentActiveSkill, laneIndex);
    }

    int resolveTrackIdx() const
    {
        if (instrumentSession == nullptr) return -1;
        for (const auto& info : instrumentSession->getTracks())
            if (info.part == currentActivePart)
                return info.sourceTrackIndex;
        return -1;
    }

    double snapQN(double rawQN) const
    {
        return ::snapQN(rawQN, currentStepDivision, currentTuplet, snapEnabledFlag);
    }

    // Patch-aware note operations — patching is automatic, sub-controllers
    // never touch OptimisticPatchBuffer directly.
    bool createNote(int trackIdx, double qn, int pitch, int lane)
    {
        if (findNote(trackIdx, qn, pitch).noteIndex >= 0)
        {
            DBG("createNote: duplicate at QN=" + juce::String(qn, 4) + " pitch=" + juce::String(pitch));
            return false;
        }
        if (!noteEditor.createNote(trackIdx, qn, pitch))
        {
            DBG("createNote: noteEditor rejected QN=" + juce::String(qn, 4) + " pitch=" + juce::String(pitch));
            return false;
        }
        patchAdd(lane, qn);
        return true;
    }

    bool eraseNote(int trackIdx, double qn, int pitch, bool drums, int lane, SkillLevel skill)
    {
        if (!noteEditor.eraseNoteAt(trackIdx, qn, pitch, drums, lane, skill)) return false;
        patchRemove(lane, qn);
        return true;
    }

    bool moveNote(int trackIdx, double oldQN, int oldPitch, int oldLane,
                  double newQN, double newEndQN, int newPitch, int newLane)
    {
        if (!noteEditor.moveNote(trackIdx, oldQN, oldPitch, newQN, newEndQN, newPitch)) return false;
        patchRemove(oldLane, oldQN);
        patchAdd(newLane, newQN);
        return true;
    }

    bool truncateNote(int trackIdx, double qn, int pitch)
    {
        return noteEditor.truncateNote(trackIdx, qn, pitch);
    }

    bool extendNote(int trackIdx, double startQN, double endQN, int pitch)
    {
        return noteEditor.extendNote(trackIdx, startQN, endQN, pitch);
    }

    bool chainExtendNotes(int trackIdx, double startQN, double endQN, int pitch)
    {
        return noteEditor.chainExtendNotes(trackIdx, startQN, endQN, pitch);
    }

    MidiWriter::NoteInfo findNote(int trackIdx, double qn, int pitch)
    {
        return noteEditor.findNote(trackIdx, qn, pitch);
    }

    std::vector<MidiWriter::NoteInfo> findNotesInRange(int trackIdx, double startQN, double endQN, int pitch)
    {
        return noteEditor.findNotesInRange(trackIdx, startQN, endQN, pitch);
    }

    int resolveBarPitch() const
    {
        return isDrums()
            ? InstrumentMapper::columnToDrumPitch(currentActiveSkill, 0, kick2xEnabled)
            : InstrumentMapper::columnToGuitarPitch(currentActiveSkill, 0);
    }

    bool createBarNote(int trackIdx, double qn)
    {
        int pitch = resolveBarPitch();
        if (pitch < 0) return false;
        if (findNote(trackIdx, qn, pitch).noteIndex >= 0)
        {
            DBG("createBarNote: duplicate at QN=" + juce::String(qn, 4) + " pitch=" + juce::String(pitch));
            return false;
        }
        if (!noteEditor.createNote(trackIdx, qn, pitch))
        {
            DBG("createBarNote: noteEditor rejected QN=" + juce::String(qn, 4) + " pitch=" + juce::String(pitch));
            return false;
        }
        patchAdd(0, qn);
        return true;
    }

    bool eraseBarNote(int trackIdx, double qn)
    {
        int pitch = resolveBarPitch();
        if (pitch < 0) return false;
        if (!noteEditor.eraseNoteAt(trackIdx, qn, pitch, isDrums(), 0, currentActiveSkill)) return false;
        patchRemove(0, qn);
        return true;
    }

    bool noteEditorAvailable() const { return noteEditor.isAvailable(); }
    void beginBatch(const char* desc) { noteEditor.beginBatch(desc); }
    void endBatch() { noteEditor.endBatch(); }

    InstrumentSession*      instrumentSession    = nullptr;
    const bool*             playingStatePtr      = nullptr;
    Part                    currentActivePart    = Part::GUITAR;
    SkillLevel              currentActiveSkill   = SkillLevel::EXPERT;
    int                     currentStepDivision  = 8;
    int                     currentTuplet        = 0;
    bool                    snapEnabledFlag      = true;
    bool                    barModeFlag          = false;
    bool                    kick2xEnabled        = false;
    CommandMapper           commandMapper;
    OverlayState            overlayState;

private:
    void patchAdd(int lane, double qn)    { if (patchBuffer) patchBuffer->addAdd(lane, qn); }
    void patchRemove(int lane, double qn) { if (patchBuffer) patchBuffer->addRemove(lane, qn); }

    NoteEditor              noteEditor;
    OptimisticPatchBuffer*  patchBuffer = nullptr;
};
