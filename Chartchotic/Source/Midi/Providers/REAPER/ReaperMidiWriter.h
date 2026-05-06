/*
  ==============================================================================

    ReaperMidiWriter.h
    REAPER implementation of MidiWriter

    Wraps REAPER API calls with undo blocks, sort management, and dirty
    marking. Item management (find/extend/create) is delegated to
    ReaperItemManager.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../MidiWriter.h"
#include "ReaperApiHelpers.h"
#include "ReaperItemManager.h"

class ReaperMidiWriter : public MidiWriter
{
public:
    ReaperMidiWriter(const ReaperAPIs& apis,
                     std::function<void*(const char*)> reaperGetFunc);
    ~ReaperMidiWriter() override = default;

    bool isAvailable() const override;

    bool insertNote(int trackIndex, double startQN, double endQN,
                   int channel, int pitch, int velocity) override;
    bool deleteNote(int trackIndex, int noteIndex) override;
    bool moveNote(int trackIndex, int noteIndex,
                 double newStartQN, double newEndQN, int newPitch) override;

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
    ReaperItemManager itemManager;

    void beginUndoBlock(void* project, const char* description);
    void endUndoBlock(void* project, const char* description);

    bool inBatch = false;
    void* batchProject = nullptr;
    void* batchTake = nullptr;
    juce::String batchDescription;

    juce::CriticalSection writeLock;
};
