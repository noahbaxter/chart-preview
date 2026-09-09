#pragma once

#include <JuceHeader.h>
#include "ReaperApiHelpers.h"
#include "../MidiWriter.h"

#include <vector>

class ReaperItemManager
{
public:
    ReaperItemManager(const ReaperAPIs& apis, std::function<void*(const char*)> getFunc);

    // Resolve a project QN range to a MIDI take. Extends the closest item
    // or creates a new one if nothing covers the range. Write operations only.
    void* getTakeForWrite(void* project, int trackIndex,
                          double startQN, double endQN);

    // Search ALL MIDI takes on the track for a note matching targetQN/pitch.
    // Caches the found take — retrieve via getLastFoundTake().
    MidiWriter::NoteInfo findNote(void* project, int trackIndex,
                                  double positionQN, int pitch);

    std::vector<MidiWriter::NoteInfo> findNotesInRange(void* project, int trackIndex,
                                                        double startQN, double endQN, int pitch);

    void* getLastFoundTake() const { return lastFoundTake; }

private:
    const ReaperAPIs& apis;
    std::function<void*(const char*)> getReaperApi;
    void* lastFoundTake = nullptr;

    struct MidiItemInfo { void* item; void* take; double pos; double len; };
    std::vector<MidiItemInfo> collectMidiItems(void* project, int trackIndex);
};
