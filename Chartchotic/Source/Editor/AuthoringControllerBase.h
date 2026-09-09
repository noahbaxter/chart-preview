#pragma once

#include <JuceHeader.h>
#include "AuthoringTypes.h"
#include "AuthoringConfig.h"
#include "AuthoringUtils.h"
#include "CommandMapper.h"
#include "NoteEditor.h"
#include "../UI/ControlConstants.h"
#include "../Midi/InstrumentSession.h"
#include "../Midi/Utils/InstrumentMapper.h"
#include "../Midi/Utils/MidiConstants.h"
#include "../Midi/Utils/GemCalculator.h"

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
    bool isElite()   const { return getRenderType(currentActivePart) == RenderType::ELITE_DRUMS; }

    // Null for a part that isn't authorable. Write mode is only offered for parts that are,
    // so the controllers deref it directly and a null would be a loud bug rather than a
    // silently wrong instrument.
    const AuthoringConfig* authoring() const { return getAuthoringConfig(currentActivePart); }

    int maxLane() const
    {
        const auto* cfg = authoring();
        if (cfg == nullptr) return 0;
        return kick2xEnabled ? cfg->highestLaneKick2x : cfg->highestLane;
    }

    // Gem for a note whose properties we already hold, as opposed to
    // resolveGhostGem which reads the current toolbar state. Routed through
    // GemCalculator with the same (Dynamic)velocity cast the render pipeline
    // uses, so a copied note previews as exactly what it will paste as.
    Gem resolveCapturedGem(int lane, int velocity, uint32_t markerMask) const
    {
        if (isDrums())
        {
            // Cymbal is the absence of the tom marker, which is slot 0. Elite has no tom
            // marker at all (lane decides), so authorsCymbal ignores the mask there.
            bool cymbal = authorsCymbal((uint)lane, currentActivePart, (markerMask & 1u) == 0);
            return GemCalculator::resolveDrumGem(cymbal, true, (Dynamic)velocity);
        }
        // Guitar slots follow modifierMarkerPitches order: hopo, strum, tap.
        return GemCalculator::resolveGuitarGem(false, false,
                                               (markerMask & (1u << 0)) != 0,
                                               (markerMask & (1u << 1)) != 0,
                                               (markerMask & (1u << 2)) != 0);
    }

    Gem resolveGhostGem(int lane) const
    {
        // Same GemCalculator call resolveCapturedGem and the parse pipeline use, fed the same
        // velocity this click will write. The switch that used to live here duplicated
        // resolveDrumGem exactly, which is how the preview drifted from what landed.
        if (isDrums())
            return GemCalculator::resolveDrumGem(
                authorsCymbal((uint)lane, currentActivePart, cymbalModeFlag),
                true, (Dynamic)resolveVelocity());
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

    int resolvePitch(int laneIndex) const
    {
        const auto* cfg = authoring();
        return cfg ? cfg->laneToPitch(currentActiveSkill, laneIndex, kick2xEnabled) : -1;
    }

    int resolveActivePitch(int laneIndex) const
    {
        return barModeFlag ? resolveBarPitch(laneIndex) : resolvePitch(laneIndex);
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
    void eraseConflictingKick(int trackIdx, double qn, int pitch)
    {
        const auto* cfg = authoring();
        if (cfg == nullptr || !kick2xEnabled) return;
        if (!cfg->isKickPitch((uint)pitch, currentActiveSkill)) return;
        auto other = cfg->conflictingKick(pitch, currentActiveSkill);
        auto conflict = findNote(trackIdx, qn, other.pitch);
        if (conflict.noteIndex >= 0 && std::abs(conflict.startQN - qn) < kQNEpsilon)
        {
            noteEditor.eraseNoteAt(trackIdx, qn, other.pitch, currentActivePart, other.lane, currentActiveSkill);
            patchRemove(other.lane, qn);
        }
    }

    bool createNote(int trackIdx, double qn, int pitch, int lane, int velocity = 100, double duration = 0.0)
    {
        auto existing = findNote(trackIdx, qn, pitch);
        if (existing.noteIndex >= 0 && std::abs(existing.startQN - qn) < kQNEpsilon)
            eraseNote(trackIdx, qn, pitch, lane, currentActiveSkill);
        eraseConflictingKick(trackIdx, qn, pitch);
        if (!noteEditor.createNote(trackIdx, qn, pitch, velocity, duration))
        {
            DBG("createNote: noteEditor rejected QN=" + juce::String(qn, 4) + " pitch=" + juce::String(pitch));
            return false;
        }
        patchAdd(lane, qn, resolveGhostGem(lane));
        ensureChartDynamics(trackIdx, velocity);
        ensureEnhancedOpens(trackIdx, lane);
        return true;
    }

    // Feature-flag text events: the notes are inert until the chart carries the
    // flag, so placing one writes it rather than leaving the charter to
    // remember. Cached per track, since the check costs a REAPER scan.
    //
    // Written bare despite the spec tables showing brackets. For dynamics every
    // chart that carries it writes it bare (17 of 17 across three independent
    // sources); for opens the spec itself allows either.
    void ensureTrackFlag(int trackIdx, const char* event, int& ensuredTrack)
    {
        if (ensuredTrack == trackIdx) return;
        if (auto* writer = noteEditor.getMidiWriter())
            if (writer->ensureTrackTextEvent(trackIdx, event))
                ensuredTrack = trackIdx;
    }

    void ensureChartDynamics(int trackIdx, int velocity)
    {
        if (!isDrums()) return;
        if (velocity != (int)Dynamic::GHOST && velocity != (int)Dynamic::ACCENT) return;
        ensureTrackFlag(trackIdx, "ENABLE_CHART_DYNAMICS", dynamicsEnsuredTrack);
    }

    // Note-based opens are off by default because note 59 is left-hand
    // animation data in GH2/RB charts. 5-fret only: 6-fret opens have been
    // note-based from the start and need no flag.
    void ensureEnhancedOpens(int trackIdx, int lane)
    {
        if (lane != 0 || !isGuitarLike(currentActivePart)) return;
        ensureTrackFlag(trackIdx, "ENHANCED_OPENS", opensEnsuredTrack);
    }

    static constexpr const char* kPlaceNoteUndo = "Chartchotic: Place note";

    // Opens a batch only when one is not already open, and closes only what it
    // opened. A lone placement is its own undo point; the same call inside a
    // paint or paste stroke folds into that stroke instead of splitting it.
    class BatchScope
    {
    public:
        BatchScope(AuthoringControllerBase& c, const char* desc) : owner(c)
        {
            opened = !owner.noteEditor.isBatching();
            if (opened) owner.beginBatch(desc);
        }
        ~BatchScope() { if (opened) owner.endBatch(); }
        BatchScope(const BatchScope&) = delete;
        BatchScope& operator=(const BatchScope&) = delete;

    private:
        AuthoringControllerBase& owner;
        bool opened = false;
    };

    // Every placement goes through here. A note and its tom/cymbal or guitar
    // force marker are separate MIDI notes, so they share one undo block and
    // one placement never costs two undos.
    bool placeNote(int trackIdx, double qn, int pitch, int lane, int velocity,
                   double duration = 0.0)
    {
        BatchScope batch(*this, kPlaceNoteUndo);
        if (!createNote(trackIdx, qn, pitch, lane, velocity, duration)) return false;
        writeMarkers(trackIdx, qn, lane);
        return true;
    }

    // Paste variant: markers come from the captured note's mask rather than
    // the current toolbar state, so a round trip reproduces the original.
    bool placeStampNote(int trackIdx, double qn, int pitch, int lane, int velocity,
                        double duration, uint32_t markerMask)
    {
        BatchScope batch(*this, kPlaceNoteUndo);
        if (!createNote(trackIdx, qn, pitch, lane, velocity, duration)) return false;
        writeMarkerMask(trackIdx, qn, lane, markerMask);
        return true;
    }

    // The part comes from currentActivePart rather than a caller-passed flag: every caller
    // was forwarding its own isDrums(), and threading instrument facts by hand is what put
    // elite's 2x kick on the 4-lane column in the first place.
    bool eraseNote(int trackIdx, double qn, int pitch, int lane, SkillLevel skill)
    {
        if (!noteEditor.eraseNoteAt(trackIdx, qn, pitch, currentActivePart, lane, skill)) return false;
        patchRemove(lane, qn);
        if (!barModeFlag)
            eraseConflictingKick(trackIdx, qn, pitch);
        return true;
    }

    bool moveNote(int trackIdx, double oldQN, int oldPitch, int oldLane,
                  double newQN, double newEndQN, int newPitch, int newLane)
    {
        if (!noteEditor.moveNote(trackIdx, oldQN, oldPitch, newQN, newEndQN, newPitch)) return false;
        patchRemove(oldLane, oldQN);
        // SelectedNote carries no gem, so the moved note's own dynamic isn't available here.
        // The lane still decides cymbal-ness, which is what sets the Z offset, so the preview
        // lands at the right height; only a moved ghost/accent previews as the toolbar's.
        patchAdd(newLane, newQN, resolveGhostGem(newLane));
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

        // Bar mode walks the rect's two edge lanes, everything else walks the
        // range, but the head/body test is the same either way, so it lives
        // here once and cannot drift between the two.
        auto collect = [&](int lane, int pitch)
        {
            if (pitch < 0) return;
            auto notes = findNotesInRange(trackIdx,
                                          std::max(0.0, rect.qnLo - kSustainLookbackQN),
                                          rect.qnHi, pitch);
            for (const auto& n : notes)
            {
                bool headIn = n.startQN >= rect.qnLo - kQNEpsilon
                           && n.startQN <= rect.qnHi + kQNEpsilon;
                bool hasSustain = (n.endQN - n.startQN) >= double(MIDI_MIN_SUSTAIN_LENGTH);
                bool bodyOverlaps = hasSustain
                                 && n.endQN > rect.qnLo + kQNEpsilon
                                 && n.startQN < rect.qnLo - kQNEpsilon;
                if (headIn)
                    result.push_back({ n, lane, false });
                else if (bodyOverlaps)
                    result.push_back({ n, lane, true });
            }
        };

        if (barModeFlag)
        {
            for (int barLane : {rect.laneLo, rect.laneHi})
                collect(barLane, resolveBarPitch(barLane));
            return result;
        }

        for (int lane = rect.laneLo; lane <= rect.laneHi; ++lane)
            collect(lane, resolveActivePitch(lane));
        return result;
    }

    int resolveBarPitch(int barLane = 0) const
    {
        return isDrums() ? resolvePitch(barLane)
                         : InstrumentMapper::columnToGuitarPitch(currentActiveSkill, 0);
    }

    bool createBarNote(int trackIdx, double qn, int barLane = 0)
    {
        int pitch = resolveBarPitch(barLane);
        if (pitch < 0) return false;
        return createNote(trackIdx, qn, pitch, barLane, resolveVelocity());
    }

    bool eraseBarNote(int trackIdx, double qn, int barLane = 0)
    {
        int pitch = resolveBarPitch(barLane);
        if (pitch < 0) return false;
        if (!noteEditor.eraseNoteAt(trackIdx, qn, pitch, currentActivePart, barLane, currentActiveSkill)) return false;
        patchRemove(barLane, qn);
        return true;
    }

    bool noteEditorAvailable() const { return noteEditor.isAvailable(); }
    void beginBatch(const char* desc) { noteEditor.beginBatch(desc); }
    void endBatch() { noteEditor.endBatch(); }
    void resolveOverlapsAt(int trackIdx, double startQN, int pitch)
    { noteEditor.resolveOverlapsAt(trackIdx, startQN, pitch); }

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
        // Elite has no tom markers: cymbal-ness is fixed by lane. Its lanes 2/3/4 are real
        // hand lanes, and 110/111/112 are its ROLL LANES, so answering here would have
        // every placement on those lanes write a stray roll.
        if (isElite()) return -1;
        using Drums = MidiPitchDefinitions::Drums;
        switch (lane)
        {
            case 2: return (int)Drums::TOM_YELLOW;
            case 3: return (int)Drums::TOM_BLUE;
            case 4: return (int)Drums::TOM_GREEN;
            default: return -1;
        }
    }

    // Every marker pitch that can qualify a note in this lane, in a stable
    // order. Note type is encoded by which of these sit alongside the note:
    // drums use a tom marker (present means tom, absent means cymbal), guitar
    // uses the force markers. Copying a note means copying this whole set.
    //
    // Both capture and paste resolve this list fresh, and record only WHICH
    // slots were filled, never the raw pitches. That keeps a paste correct
    // across difficulties, since the guitar pitches are skill-dependent
    // (EXPERT_HOPO vs HARD_HOPO) but their slot positions are not.
    std::vector<int> modifierMarkerPitches(int lane) const
    {
        std::vector<int> out;
        if (isDrums())
        {
            int p = resolveTomMarkerPitch(lane);
            if (p >= 0) out.push_back(p);
            return out;
        }
        for (auto force : { GuitarForce::Hopo, GuitarForce::Strum, GuitarForce::Tap })
        {
            int p = resolveGuitarForcePitchFor(force);
            if (p >= 0) out.push_back(p);
        }
        return out;
    }

    // Bit i is set when modifierMarkerPitches(lane)[i] is present at qn.
    uint32_t captureMarkerMask(int trackIdx, double qn, int lane)
    {
        uint32_t mask = 0;
        auto candidates = modifierMarkerPitches(lane);
        for (size_t i = 0; i < candidates.size(); ++i)
            if (findNote(trackIdx, qn, candidates[i]).noteIndex >= 0)
                mask |= (1u << i);
        return mask;
    }

    // Reproduces a captured mask exactly, creating missing markers and erasing
    // stray ones, so a pasted note ends up the type it was copied from rather
    // than inheriting whatever the toolbar is set to.
    void writeMarkerMask(int trackIdx, double qn, int lane, uint32_t mask)
    {
        auto candidates = modifierMarkerPitches(lane);
        for (size_t i = 0; i < candidates.size(); ++i)
        {
            bool want = (mask & (1u << i)) != 0;
            auto existing = findNote(trackIdx, qn, candidates[i]);
            if (want && existing.noteIndex < 0)
                createMarkerNote(trackIdx, qn, candidates[i]);
            else if (!want && existing.noteIndex >= 0)
                eraseNote(trackIdx, qn, candidates[i], lane, currentActiveSkill);
        }
    }

    // Markers the toolbar asks for. Drums have one slot and cymbal is its
    // absence; guitar force is one-of.
    uint32_t currentMarkerMask(int lane) const
    {
        if (isDrums())
            return (resolveTomMarkerPitch(lane) >= 0 && !cymbalModeFlag) ? 1u : 0u;

        uint32_t mask = 0;
        int slot = 0;
        for (auto force : { GuitarForce::Hopo, GuitarForce::Strum, GuitarForce::Tap })
        {
            if (resolveGuitarForcePitchFor(force) < 0) continue;
            if (force == currentGuitarForce) mask |= (1u << slot);
            ++slot;
        }
        return mask;
    }

    // One path for every instrument, and it clears what it does not want, so
    // re-placing a note as a plainer type actually plains it.
    void writeMarkers(int trackIdx, double qn, int lane)
    {
        writeMarkerMask(trackIdx, qn, lane, currentMarkerMask(lane));
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

    InstrumentSession*      instrumentSession    = nullptr;
    const bool*             playingStatePtr      = nullptr;
    Part                    currentActivePart    = Part::GUITAR;
    SkillLevel              currentActiveSkill   = SkillLevel::EXPERT;
    int                     currentStepDivision  = 8;
    int                     currentTuplet        = 0;
    bool                    snapEnabledFlag      = true;
    bool                    barModeFlag          = false;
    bool                    kick2xEnabled        = false;
    int                     dynamicsEnsuredTrack = -1;
    int                     opensEnsuredTrack    = -1;
    DrumDynamic             currentDrumDynamic   = DrumDynamic::Normal;
    GuitarForce             currentGuitarForce   = GuitarForce::None;
    bool                    cymbalModeFlag       = false;
    CommandMapper           commandMapper;
    OverlayState            overlayState;

private:
    void patchAdd(int lane, double qn, Gem gem) { if (patchBuffer) patchBuffer->addAdd(lane, qn, gem); }
    void patchRemove(int lane, double qn) { if (patchBuffer) patchBuffer->addRemove(lane, qn); }

    NoteEditor              noteEditor;
    OptimisticPatchBuffer*  patchBuffer = nullptr;
};
