/*
  ==============================================================================

    ReaperMidiWriter.h
    REAPER implementation of MidiWriter

    Wraps REAPER API calls with undo blocks, sort management, and dirty
    marking. After any mutation, MarkProjectDirty changes the track hash
    so the existing read pipeline auto-refetches on the next process() cycle.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../MidiWriter.h"
#include "ReaperApiHelpers.h"

class ReaperMidiWriter : public MidiWriter
{
public:
    ReaperMidiWriter(const ReaperAPIs& apis,
                     std::function<void*(const char*)> reaperGetFunc);
    ~ReaperMidiWriter() override = default;

    bool isAvailable() const override;

    // Single-note operations
    bool insertNote(int trackIndex, double startPPQ, double endPPQ,
                   int channel, int pitch, int velocity) override;
    bool deleteNote(int trackIndex, int noteIndex) override;
    bool moveNote(int trackIndex, int noteIndex,
                 double newStartPPQ, double newEndPPQ, int newPitch) override;

    // Batch operations
    void beginBatch(const char* undoDescription) override;
    bool batchInsertNote(int trackIndex, double startPPQ, double endPPQ,
                        int channel, int pitch, int velocity) override;
    bool batchDeleteNote(int trackIndex, int noteIndex) override;
    bool batchMoveNote(int trackIndex, int noteIndex,
                      double newStartPPQ, double newEndPPQ, int newPitch) override;
    void endBatch() override;

private:
    const ReaperAPIs& apis;
    std::function<void*(const char*)> getReaperApi;

    // Resolve the first MIDI take on the given track
    void* getFirstMidiTake(void* project, int trackIndex);

    // Undo/sort helpers
    void beginUndoBlock(void* project, const char* description);
    void endUndoBlock(void* project, const char* description);

    // Batch state
    bool inBatch = false;
    void* batchProject = nullptr;
    void* batchTake = nullptr;
    juce::String batchDescription;

    juce::CriticalSection writeLock;
};
