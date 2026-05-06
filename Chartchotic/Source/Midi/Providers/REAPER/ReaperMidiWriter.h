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
    bool insertNote(int trackIndex, double startQN, double endQN,
                   int channel, int pitch, int velocity) override;
    bool deleteNote(int trackIndex, int noteIndex) override;
    bool moveNote(int trackIndex, int noteIndex,
                 double newStartQN, double newEndQN, int newPitch) override;

    // Batch operations
    void beginBatch(const char* undoDescription) override;
    bool batchInsertNote(int trackIndex, double startQN, double endQN,
                        int channel, int pitch, int velocity) override;
    bool batchDeleteNote(int trackIndex, int noteIndex) override;
    bool batchMoveNote(int trackIndex, int noteIndex,
                      double newStartQN, double newEndQN, int newPitch) override;
    void endBatch() override;

private:
    const ReaperAPIs& apis;
    std::function<void*(const char*)> getReaperApi;

    // Resolve the first MIDI take on the given track
    void* getFirstMidiTake(void* project, int trackIndex);

    // Same as getFirstMidiTake, but also returns the owning item via outItem.
    void* getFirstMidiTakeAndItem(void* project, int trackIndex, void** outItem);

    // Ensure the active MIDI item on `track` covers the project-time range
    // [startQN, endQN]. Extends D_LENGTH (and D_POSITION if needed) if an
    // item exists; creates a new MIDI item if the track has none. Returns
    // the take to write into, or nullptr on failure.
    void* ensureItemCoversRange(void* project, int trackIndex,
                                double startQN, double endQN);

    // Lowers D_POSITION and/or grows D_LENGTH on `item`, while keeping existing
    // MIDI notes anchored at their original project-time positions when the
    // position changes. `take` must be the active take of `item`.
    void resizeItemKeepingNoteTimes(void* item, void* take,
                                    double oldPos, double newPos,
                                    double oldLen, double newLen);

    // Disable REAPER's "Loop source" on the item so extending D_LENGTH past the
    // MIDI source's natural end leaves blank space instead of replicating
    // content. No-op if SetMediaItemInfo_Value isn't available.
    void disableSourceLoop(void* item);

    // Undo/sort helpers
    void beginUndoBlock(void* project, const char* description);
    void endUndoBlock(void* project, const char* description);

    // Batch state
    bool inBatch = false;
    void* batchProject = nullptr;
    void* batchTake = nullptr;
    juce::String batchDescription;

    juce::CriticalSection writeLock;

    // Cached last-known covering item so ensureItemCoversRange can skip the
    // full per-track enumeration when the same item still covers the range.
    struct CachedItem
    {
        int    trackIndex = -1;
        void*  item       = nullptr;
        void*  take       = nullptr;
        double pos        = 0.0;
        double len        = 0.0;
    };
    CachedItem cachedItem;
};
