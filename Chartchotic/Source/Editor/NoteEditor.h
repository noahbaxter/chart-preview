#pragma once

#include "../UI/ControlConstants.h"
#include "../Midi/Providers/MidiWriter.h"
class InstrumentSession;

class NoteEditor
{
public:
    void setMidiWriter(MidiWriter* w)            { midiWriter = w; }
    void setInstrumentSession(InstrumentSession* s) { instrumentSession = s; }

    bool isAvailable() const;

    bool createNote  (int trackIdx, double startQN, int pitch);
    bool eraseNoteAt (int trackIdx, double rawQN, int pitch,
                      bool drums, int lane, SkillLevel skill);
    bool truncateNote(int trackIdx, double noteStartQN, int pitch);
    bool extendNote  (int trackIdx, double startQN, double endQN, int pitch);
    MidiWriter::NoteInfo findNote(int trackIdx, double qn, int pitch);

    bool chainExtendNotes(int trackIdx, double startQN, double endQN, int pitch);

    void beginBatch(const char* description);
    void endBatch();
    bool isBatching() const { return batchActive; }

private:
    MidiWriter*        midiWriter        = nullptr;
    InstrumentSession* instrumentSession = nullptr;
    bool               batchActive       = false;
};
