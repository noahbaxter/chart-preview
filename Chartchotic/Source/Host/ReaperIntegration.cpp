/*
  ==============================================================================

    ReaperIntegration.cpp
    REAPER timeline MIDI processing — raw storage only, no gem calculation.

  ==============================================================================
*/

#include "ReaperIntegration.h"
#include "../PluginProcessor.h"
#include "../Utils/ChartTypes.h"

void ReaperIntegration::processReaperTimelineMidi(
    ChartchoticAudioProcessor& processor,
    PPQ startPPQ,
    PPQ endPPQ,
    double bpm,
    uint timeSignatureNumerator,
    uint timeSignatureDenominator)
{
    if (!processor.isReaperHost || !processor.reaperMidiProvider.isReaperApiAvailable())
        return;

    auto& midiProcessor = processor.getMidiProcessor();

    // Fetch from the REAPER timeline FIRST, with NO lock held: this is a slow API call and must
    // not sit between the clear and the write.
    auto reaperNotes = processor.reaperMidiProvider.getNotesInRange(startPPQ.toDouble(), endPPQ.toDouble());

    // Clear + write the range under ONE lock so the render thread never reads the cleared-but-not-
    // yet-rewritten gap. Splitting them (clear under its own lock, then the slow REAPER fetch, then
    // write under a second lock) let a render land in the gap and drop every gem in the range --
    // most visible on dense chords (multiple toms/cymbals). CriticalSection is recursive, so
    // clearNoteDataInRange's own lock re-enters harmlessly. Same cause as the v0.8.6 flicker bug.
    {
        const juce::ScopedLock lock(midiProcessor.noteStateMapLock);

        midiProcessor.clearNoteDataInRange(startPPQ, endPPQ);

        for (const auto& reaperNote : reaperNotes)
        {
            if (reaperNote.muted) continue;

            uint pitch = reaperNote.pitch;
            PPQ noteStartPPQ = PPQ(reaperNote.startPPQ);
            PPQ noteEndPPQ = PPQ(reaperNote.endPPQ);

            midiProcessor.noteStateMapArray[pitch][noteStartPPQ] = NoteData(reaperNote.velocity);
            midiProcessor.noteStateMapArray[pitch][noteEndPPQ - PPQ(1)] = NoteData(0);
        }
    }
}
