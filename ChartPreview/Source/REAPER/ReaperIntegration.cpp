/*
  ==============================================================================

    ReaperIntegration.cpp
    REAPER timeline MIDI processing

  ==============================================================================
*/

#include "ReaperIntegration.h"
#include "../PluginProcessor.h"
#include "../Utils/Utils.h"

void ReaperIntegration::processReaperTimelineMidi(
    ChartPreviewAudioProcessor& processor,
    PPQ startPPQ,
    PPQ endPPQ,
    double bpm,
    uint timeSignatureNumerator,
    uint timeSignatureDenominator)
{
    if (!processor.isReaperHost || !processor.reaperMidiProvider.isReaperApiAvailable())
        return;

    auto& state = processor.getState();
    auto& midiProcessor = processor.getMidiProcessor();

    // Only log when range changes to avoid spam
    static PPQ lastLoggedStartPPQ = PPQ(-1.0);
    static PPQ lastLoggedEndPPQ = PPQ(-1.0);
    bool shouldLog = (startPPQ != lastLoggedStartPPQ || endPPQ != lastLoggedEndPPQ);

    if (shouldLog)
    {
        processor.print("=== REAPER Timeline Fetch ===");
        processor.print("Start PPQ: " + juce::String(startPPQ.toDouble()));
        processor.print("End PPQ: " + juce::String(endPPQ.toDouble()));
        processor.print("BPM: " + juce::String(bpm));
        processor.print("Time Sig: " + juce::String(timeSignatureNumerator) + "/" + juce::String(timeSignatureDenominator));
        lastLoggedStartPPQ = startPPQ;
        lastLoggedEndPPQ = endPPQ;
    }

    // Clear existing note data in this range to get fresh data from timeline
    midiProcessor.clearNoteDataInRange(startPPQ, endPPQ);

    // Get notes from REAPER timeline
    auto reaperNotes = processor.reaperMidiProvider.getNotesInRange(startPPQ.toDouble(), endPPQ.toDouble());

    if (shouldLog)
    {
        processor.print("=== REAPER NOTES RECEIVED ===");
        processor.print("Found " + juce::String(reaperNotes.size()) + " notes in range");

        // Analyze pitch ranges
        int validDrumNotes = 0;
        int validGuitarNotes = 0;
        int invalidNotes = 0;
        int minPitch = 127, maxPitch = 0;

        for (const auto& note : reaperNotes)
        {
            if (note.pitch < minPitch) minPitch = note.pitch;
            if (note.pitch > maxPitch) maxPitch = note.pitch;

            // Check if pitch is in valid ranges
            bool isDrumNote = (note.pitch >= 60 && note.pitch <= 64) ||  // Easy
                             (note.pitch >= 72 && note.pitch <= 76) ||  // Medium
                             (note.pitch >= 84 && note.pitch <= 88) ||  // Hard
                             (note.pitch >= 96 && note.pitch <= 112) || // Expert + toms
                             (note.pitch == 116) || (note.pitch >= 126); // SP, lanes

            bool isGuitarNote = (note.pitch >= 59 && note.pitch <= 66) ||  // Easy
                               (note.pitch >= 71 && note.pitch <= 78) ||  // Medium
                               (note.pitch >= 83 && note.pitch <= 90) ||  // Hard
                               (note.pitch >= 95 && note.pitch <= 104) || // Expert
                               (note.pitch == 116) || (note.pitch >= 126); // SP, lanes

            if (isDrumNote) validDrumNotes++;
            if (isGuitarNote) validGuitarNotes++;
            if (!isDrumNote && !isGuitarNote) invalidNotes++;
        }

        processor.print("Pitch analysis:");
        processor.print("  Range: " + juce::String(minPitch) + " to " + juce::String(maxPitch));
        processor.print("  Valid drum pitches: " + juce::String(validDrumNotes));
        processor.print("  Valid guitar pitches: " + juce::String(validGuitarNotes));
        processor.print("  Invalid pitches: " + juce::String(invalidNotes));

        // Log first 5 notes to see what we're getting
        processor.print("First 5 notes:");
        int logCount = std::min(5, (int)reaperNotes.size());
        for (int i = 0; i < logCount; i++)
        {
            const auto& note = reaperNotes[i];
            processor.print("  " + juce::String(i) + ": pitch=" + juce::String(note.pitch) +
                  " vel=" + juce::String(note.velocity) +
                  " start=" + juce::String(note.startPPQ, 3) +
                  " end=" + juce::String(note.endPPQ, 3) +
                  " ch=" + juce::String(note.channel));
        }
    }

    // Get current skill level for filtering
    SkillLevel currentSkill = (SkillLevel)((int)state.getProperty("skillLevel"));

    int notesProcessed = 0;
    int notesSkipped = 0;

    // Get valid pitches for current instrument and skill level
    std::vector<uint> validPlayablePitches;
    std::vector<uint> validModifierPitches;

    if (isPart(state, Part::DRUMS))
    {
        validPlayablePitches = MidiUtility::getDrumPitchesForSkill(currentSkill);
        // Drum modifiers: tom markers (110-112), star power (116), lanes (126-127)
        validModifierPitches = {110, 111, 112, 116, 126, 127};
    }
    else if (isPart(state, Part::GUITAR))
    {
        validPlayablePitches = MidiUtility::getGuitarPitchesForSkill(currentSkill);
        // Guitar modifiers: HOPO/STRUM (per difficulty), TAP (104), star power (116), lanes (126-127)
        using Guitar = MidiPitchDefinitions::Guitar;
        switch (currentSkill)
        {
            case SkillLevel::EXPERT:
                validModifierPitches = {(uint)Guitar::EXPERT_HOPO, (uint)Guitar::EXPERT_STRUM, (uint)Guitar::TAP, (uint)Guitar::SP, (uint)Guitar::LANE_1, (uint)Guitar::LANE_2};
                break;
            case SkillLevel::HARD:
                validModifierPitches = {(uint)Guitar::HARD_HOPO, (uint)Guitar::HARD_STRUM, (uint)Guitar::TAP, (uint)Guitar::SP, (uint)Guitar::LANE_1, (uint)Guitar::LANE_2};
                break;
            case SkillLevel::MEDIUM:
                validModifierPitches = {(uint)Guitar::MEDIUM_HOPO, (uint)Guitar::MEDIUM_STRUM, (uint)Guitar::TAP, (uint)Guitar::SP, (uint)Guitar::LANE_1, (uint)Guitar::LANE_2};
                break;
            case SkillLevel::EASY:
                validModifierPitches = {(uint)Guitar::EASY_HOPO, (uint)Guitar::EASY_STRUM, (uint)Guitar::TAP, (uint)Guitar::SP, (uint)Guitar::LANE_1, (uint)Guitar::LANE_2};
                break;
        }
    }

    // FIRST PASS: Process modifier pitches (tom markers, HOPO/STRUM, star power, lanes)
    // These must be processed first because gem type calculation depends on them
    for (const auto& reaperNote : reaperNotes)
    {
        if (reaperNote.muted) continue;
        uint pitch = reaperNote.pitch;

        // Check if this is a valid modifier pitch
        bool isModifier = std::find(validModifierPitches.begin(), validModifierPitches.end(), pitch) != validModifierPitches.end();
        if (!isModifier) continue;

        PPQ noteStartPPQ = PPQ(reaperNote.startPPQ);
        PPQ noteEndPPQ = PPQ(reaperNote.endPPQ);

        // Add modifier to note state map (no gem type needed for modifiers)
        {
            const juce::ScopedLock lock(midiProcessor.noteStateMapLock);
            midiProcessor.noteStateMapArray[pitch][noteStartPPQ] = NoteData(reaperNote.velocity, Gem::NONE);
            midiProcessor.noteStateMapArray[pitch][noteEndPPQ - PPQ(1)] = NoteData(0, Gem::NONE);
        }

        if (shouldLog)
        {
            processor.print("Modifier: pitch=" + juce::String(pitch) + " at PPQ=" + juce::String(noteStartPPQ.toDouble(), 3));
        }
    }

    // SECOND PASS: Process playable notes (with proper gem type calculation)
    for (const auto& reaperNote : reaperNotes)
    {
        if (reaperNote.muted) continue; // Skip muted notes
        uint pitch = reaperNote.pitch;

        // Check if this is a valid playable pitch for current skill level
        bool isValidPlayablePitch = std::find(validPlayablePitches.begin(), validPlayablePitches.end(), pitch) != validPlayablePitches.end();

        if (!isValidPlayablePitch)
        {
            notesSkipped++;
            continue; // Skip notes from other difficulties or modifier pitches
        }

        notesProcessed++;

        // Create note-on message at note start
        juce::MidiMessage noteOnMsg = juce::MidiMessage::noteOn(reaperNote.channel + 1,
                                                                reaperNote.pitch,
                                                                (juce::uint8)reaperNote.velocity);
        PPQ noteStartPPQ = PPQ(reaperNote.startPPQ);

        // Process the note through MidiProcessor to calculate gem type
        uint noteNumber = noteOnMsg.getNoteNumber();
        uint velocity = noteOnMsg.getVelocity();

        Gem gemType = Gem::NONE;
        if (velocity > 0) {
            if (isPart(state, Part::GUITAR)) {
                gemType = midiProcessor.getGuitarGemType(noteNumber, noteStartPPQ);
            } else if (isPart(state, Part::DRUMS)) {
                Dynamic dynamic = (Dynamic)velocity;
                gemType = midiProcessor.getDrumGemType(noteNumber, noteStartPPQ, dynamic);
            }
        }

        // Debug logging for Expert difficulty
        if (shouldLog && currentSkill == SkillLevel::EXPERT && isPart(state, Part::DRUMS) && notesProcessed <= 10)
        {
            juce::String gemTypeName;
            switch (gemType)
            {
                case Gem::NONE: gemTypeName = "NONE"; break;
                case Gem::HOPO_GHOST: gemTypeName = "HOPO_GHOST"; break;
                case Gem::NOTE: gemTypeName = "NOTE"; break;
                case Gem::TAP_ACCENT: gemTypeName = "TAP_ACCENT"; break;
                case Gem::CYM_GHOST: gemTypeName = "CYM_GHOST"; break;
                case Gem::CYM: gemTypeName = "CYM"; break;
                case Gem::CYM_ACCENT: gemTypeName = "CYM_ACCENT"; break;
            }
            processor.print("Expert note #" + juce::String(notesProcessed) + ": pitch=" + juce::String(pitch) +
                  " vel=" + juce::String(velocity) + " → gemType=" + gemTypeName);
        }

        // Add to note state map
        {
            const juce::ScopedLock lock(midiProcessor.noteStateMapLock);
            midiProcessor.noteStateMapArray[noteNumber][noteStartPPQ] = NoteData(velocity, gemType);
        }

        // Create note-off message at note end
        PPQ noteEndPPQ = PPQ(reaperNote.endPPQ);
        {
            const juce::ScopedLock lock(midiProcessor.noteStateMapLock);
            midiProcessor.noteStateMapArray[noteNumber][noteEndPPQ - PPQ(1)] = NoteData(0, Gem::NONE);
        }
    }

    if (shouldLog)
    {
        processor.print("=== PROCESSING SUMMARY ===");
        processor.print("Notes processed: " + juce::String(notesProcessed));
        processor.print("Notes skipped (wrong difficulty): " + juce::String(notesSkipped));
        processor.print("Current skill level: " + juce::String((int)currentSkill));
    }

    // NOTE: Gridlines are now generated on-demand in the rendering code (GridlineGenerator)
    // No need to pre-build them here
}