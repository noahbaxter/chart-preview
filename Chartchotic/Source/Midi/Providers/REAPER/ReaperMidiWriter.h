/*
  ==============================================================================

    ReaperMidiWriter.h
    REAPER implementation of MidiWriter

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../MidiWriter.h"
#include "ReaperApiHelpers.h"
#include "ReaperItemManager.h"

#include <vector>

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
    bool deleteNoteAtQN(int trackIndex, int noteIndex, double hintQN) override;
    int findNoteIndex(int trackIndex, double targetQN, int pitch,
                      double toleranceQN = 0.25) override;
    bool moveNote(int trackIndex, int noteIndex,
                 double newStartQN, double newEndQN, int newPitch) override;

    void beginBatch(const char* undoDescription) override;
    bool batchInsertNote(int trackIndex, double startQN, double endQN,
                        int channel, int pitch, int velocity) override;
    bool batchDeleteNote(int trackIndex, int noteIndex, double hintQN = -1.0) override;
    bool batchMoveNote(int trackIndex, int noteIndex,
                      double newStartQN, double newEndQN, int newPitch) override;
    void endBatch() override;

private:
    const ReaperAPIs& apis;
    std::function<void*(const char*)> getReaperApi;
    ReaperItemManager itemManager;

    void endUndoBlock(void* project, const char* description);
    void addBatchTake(void* take);

    bool inBatch = false;
    void* batchProject = nullptr;
    std::vector<void*> batchTakes;
    juce::String batchDescription;

    juce::CriticalSection writeLock;
};
