#pragma once

#include <JuceHeader.h>
#include "AuthoringTypes.h"
#include "AuthoringUtils.h"
#include "CommandMapper.h"
#include "NoteEditor.h"
#include "../UI/ControlConstants.h"
#include "../Midi/InstrumentSession.h"
#include "../Midi/Utils/InstrumentMapper.h"
#include "../Midi/Utils/MidiConstants.h"

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
    void setDrumDynamic(DrumDynamic d)             { currentDrumDynamic = d; }
    void setGuitarForce(GuitarForce f)             { currentGuitarForce = f; }
    void setCymbalMode(bool c)                     { cymbalModeFlag = c; }

    Part        activePart()    const { return currentActivePart; }
    SkillLevel  activeSkill()   const { return currentActiveSkill; }
    int         stepDivision()  const { return currentStepDivision; }
    int         tuplet()        const { return currentTuplet; }
    bool        snapEnabled()   const { return snapEnabledFlag; }
    DrumDynamic drumDynamic()   const { return currentDrumDynamic; }
    GuitarForce guitarForce()   const { return currentGuitarForce; }
    bool        cymbalMode()    const { return cymbalModeFlag; }

    const OverlayState& getOverlayState() const { return overlayState; }

    std::function<void()> onStateChanged;

protected:
    bool isPlaying() const { return playingStatePtr && *playingStatePtr; }
    bool isDrums()   const { return isDrumLike(currentActivePart); }

    Gem resolveGhostGem(int lane) const
    {
        if (isDrums())
        {
            bool canBeCymbal = (lane >= 2 && lane <= 4);
            bool cymbal = canBeCymbal && cymbalModeFlag;
            switch (currentDrumDynamic)
            {
                case DrumDynamic::Ghost:  return cymbal ? Gem::CYM_GHOST  : Gem::HOPO_GHOST;
                case DrumDynamic::Accent: return cymbal ? Gem::CYM_ACCENT : Gem::TAP_ACCENT;
                default:                  return cymbal ? Gem::CYM        : Gem::NOTE;
            }
        }
        switch (currentGuitarForce)
        {
            case GuitarForce::Hopo: return Gem::HOPO_GHOST;
            case GuitarForce::Tap:  return Gem::TAP_ACCENT;
            default:                return Gem::NOTE;
        }
    }

    int resolveVelocity() const
    {
        if (!isDrums()) return 100;
        switch (currentDrumDynamic)
        {
            case DrumDynamic::Ghost:  return 1;
            case DrumDynamic::Accent: return 127;
            default:                  return 100;
        }
    }

    int resolvePitch(int laneIndex, bool drums) const
    {
        return drums
            ? InstrumentMapper::columnToDrumPitch(currentActiveSkill, laneIndex, false)
            : InstrumentMapper::columnToGuitarPitch(currentActiveSkill, laneIndex);
    }

    int resolveActivePitch(int laneIndex) const
    {
        return barModeFlag ? resolveBarPitch() : resolvePitch(laneIndex, isDrums());
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
    bool createNote(int trackIdx, double qn, int pitch, int lane, int velocity = 100)
    {
        auto existing = findNote(trackIdx, qn, pitch);
        if (existing.noteIndex >= 0 && std::abs(existing.startQN - qn) < 0.001)
        {
            DBG("createNote: duplicate at QN=" + juce::String(qn, 4) + " pitch=" + juce::String(pitch));
            return false;
        }
        if (!noteEditor.createNote(trackIdx, qn, pitch, velocity))
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

    struct ClassifiedNote
    {
        MidiWriter::NoteInfo note;
        int  lane = -1;
        bool sustainOnly = false;
    };

    std::vector<ClassifiedNote> classifyNotesInRect(int trackIdx, const MarqueeRect& rect)
    {
        std::vector<ClassifiedNote> result;
        for (int lane = rect.laneLo; lane <= rect.laneHi; ++lane)
        {
            int pitch = resolveActivePitch(lane);
            if (pitch < 0) continue;
            auto notes = findNotesInRange(trackIdx,
                                          std::max(0.0, rect.qnLo - 32.0),
                                          rect.qnHi, pitch);
            for (const auto& n : notes)
            {
                bool headIn = n.startQN >= rect.qnLo - 0.001
                           && n.startQN <= rect.qnHi + 0.001;
                bool hasSustain = (n.endQN - n.startQN) >= double(MIDI_MIN_SUSTAIN_LENGTH);
                bool bodyOverlaps = hasSustain
                                 && n.endQN > rect.qnLo + 0.001
                                 && n.startQN < rect.qnLo - 0.001;
                if (headIn)
                    result.push_back({ n, lane, false });
                else if (bodyOverlaps)
                    result.push_back({ n, lane, true });
            }
        }
        return result;
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

    bool createMarkerNote(int trackIdx, double qn, int pitch)
    {
        return noteEditor.createNote(trackIdx, qn, pitch);
    }

    bool setNoteVelocity(int trackIdx, double qn, int pitch, int velocity)
    {
        return noteEditor.setNoteVelocity(trackIdx, qn, pitch, velocity);
    }

    int resolveTomMarkerPitch(int lane) const
    {
        if (!isDrums()) return -1;
        using Drums = MidiPitchDefinitions::Drums;
        switch (lane)
        {
            case 2: return (int)Drums::TOM_YELLOW;
            case 3: return (int)Drums::TOM_BLUE;
            case 4: return (int)Drums::TOM_GREEN;
            default: return -1;
        }
    }

    void writeTomMarker(int trackIdx, double qn, int lane)
    {
        int markerPitch = resolveTomMarkerPitch(lane);
        if (markerPitch < 0) return;

        auto existing = findNote(trackIdx, qn, markerPitch);
        if (cymbalModeFlag)
        {
            if (existing.noteIndex >= 0)
                eraseNote(trackIdx, qn, markerPitch, true, lane, currentActiveSkill);
        }
        else
        {
            if (existing.noteIndex < 0)
                createMarkerNote(trackIdx, qn, markerPitch);
        }
    }

    int resolveGuitarForcePitchFor(GuitarForce force) const
    {
        if (isDrums()) return -1;
        using Guitar = MidiPitchDefinitions::Guitar;
        switch (force)
        {
            case GuitarForce::Hopo:
                switch (currentActiveSkill) {
                    case SkillLevel::EXPERT: return (int)Guitar::EXPERT_HOPO;
                    case SkillLevel::HARD:   return (int)Guitar::HARD_HOPO;
                    case SkillLevel::MEDIUM: return (int)Guitar::MEDIUM_HOPO;
                    case SkillLevel::EASY:   return (int)Guitar::EASY_HOPO;
                }
                break;
            case GuitarForce::Strum:
                switch (currentActiveSkill) {
                    case SkillLevel::EXPERT: return (int)Guitar::EXPERT_STRUM;
                    case SkillLevel::HARD:   return (int)Guitar::HARD_STRUM;
                    case SkillLevel::MEDIUM: return (int)Guitar::MEDIUM_STRUM;
                    case SkillLevel::EASY:   return (int)Guitar::EASY_STRUM;
                }
                break;
            case GuitarForce::Tap:
                return (int)Guitar::TAP;
            default:
                return -1;
        }
        return -1;
    }

    int resolveGuitarForcePitch() const
    {
        return resolveGuitarForcePitchFor(currentGuitarForce);
    }

    void writeGuitarForceMarker(int trackIdx, double qn)
    {
        int forcePitch = resolveGuitarForcePitch();
        if (forcePitch < 0) return;

        auto existing = findNote(trackIdx, qn, forcePitch);
        if (existing.noteIndex < 0)
            createMarkerNote(trackIdx, qn, forcePitch);
    }

    InstrumentSession*      instrumentSession    = nullptr;
    const bool*             playingStatePtr      = nullptr;
    Part                    currentActivePart    = Part::GUITAR;
    SkillLevel              currentActiveSkill   = SkillLevel::EXPERT;
    int                     currentStepDivision  = 8;
    int                     currentTuplet        = 0;
    bool                    snapEnabledFlag      = true;
    bool                    barModeFlag          = false;
    bool                    kick2xEnabled        = false;
    DrumDynamic             currentDrumDynamic   = DrumDynamic::Normal;
    GuitarForce             currentGuitarForce   = GuitarForce::None;
    bool                    cymbalModeFlag       = false;
    CommandMapper           commandMapper;
    OverlayState            overlayState;

private:
    void patchAdd(int lane, double qn)    { if (patchBuffer) patchBuffer->addAdd(lane, qn); }
    void patchRemove(int lane, double qn) { if (patchBuffer) patchBuffer->addRemove(lane, qn); }

    NoteEditor              noteEditor;
    OptimisticPatchBuffer*  patchBuffer = nullptr;
};
