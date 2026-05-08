/*
  ==============================================================================

    MidiWriter.h
    DAW-agnostic interface for MIDI note mutations

    Abstracts note insert/delete/move operations so the authoring layer
    doesn't depend on REAPER directly. Each DAW backend provides its own
    implementation.

  ==============================================================================
*/

#pragma once

#include <vector>

/**
 * Abstract interface for writing MIDI notes back to the host DAW.
 *
 * All time positions are project quarter notes (QN). Backends are responsible
 * for converting to take-local time domains (e.g. take PPQ in REAPER).
 *
 * Single-note operations wrap their own undo block.
 * Batch operations share one undo block between beginBatch/endBatch.
 */
class MidiWriter
{
public:
    virtual ~MidiWriter() = default;

    /** Returns true if the writer is connected and can accept mutations. */
    virtual bool isAvailable() const = 0;

    // --- Single-note operations (each creates its own undo point) ---

    virtual bool insertNote(int trackIndex, double startQN, double endQN,
                           int channel, int pitch, int velocity) = 0;

    virtual bool deleteNote(int trackIndex, int noteIndex) = 0;
    virtual bool deleteNoteAtQN(int trackIndex, int noteIndex, double hintQN) { return deleteNote(trackIndex, noteIndex); }

    struct NoteInfo {
        int    noteIndex = -1;
        double startQN = 0;
        double endQN = 0;
        int    pitch = -1;
    };

    virtual NoteInfo findNote(int trackIndex, double positionQN, int pitch)
    { (void)trackIndex; (void)positionQN; (void)pitch; return {}; }

    virtual std::vector<NoteInfo> findNotesInRange(int trackIndex, double startQN,
                                                    double endQN, int pitch)
    { (void)trackIndex; (void)startQN; (void)endQN; (void)pitch; return {}; }

    virtual bool moveNote(int trackIndex, int noteIndex,
                         double newStartQN, double newEndQN, int newPitch) = 0;

    // --- Batch operations (single undo point for all) ---

    virtual void beginBatch(const char* undoDescription) = 0;

    virtual bool batchInsertNote(int trackIndex, double startQN, double endQN,
                                int channel, int pitch, int velocity) = 0;

    virtual bool batchDeleteNote(int trackIndex, int noteIndex, double hintQN = -1.0) = 0;

    virtual bool batchMoveNote(int trackIndex, int noteIndex,
                              double newStartQN, double newEndQN, int newPitch) = 0;

    virtual void endBatch() = 0;
};
