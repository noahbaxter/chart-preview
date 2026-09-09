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

/**
 * Abstract interface for writing MIDI notes back to the host DAW.
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

    virtual bool insertNote(int trackIndex, double startPPQ, double endPPQ,
                           int channel, int pitch, int velocity) = 0;

    virtual bool deleteNote(int trackIndex, int noteIndex) = 0;

    virtual bool moveNote(int trackIndex, int noteIndex,
                         double newStartPPQ, double newEndPPQ, int newPitch) = 0;

    // --- Batch operations (single undo point for all) ---

    virtual void beginBatch(const char* undoDescription) = 0;

    virtual bool batchInsertNote(int trackIndex, double startPPQ, double endPPQ,
                                int channel, int pitch, int velocity) = 0;

    virtual bool batchDeleteNote(int trackIndex, int noteIndex) = 0;

    virtual bool batchMoveNote(int trackIndex, int noteIndex,
                              double newStartPPQ, double newEndPPQ, int newPitch) = 0;

    virtual void endBatch() = 0;
};
