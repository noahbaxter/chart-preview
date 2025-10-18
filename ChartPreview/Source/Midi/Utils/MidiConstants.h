/*
  ==============================================================================

    MidiConstants.h
    Created: 18 Oct 2024
    Author:  Noah Baxter

    MIDI processing constants including thresholds, note limits, and tuning parameters.

  ==============================================================================
*/

#pragma once

#include "../../Utils/PPQ.h"

constexpr float MIDI_RESOLUTION = 480.0f;

inline const PPQ MIDI_TICK_BASE = PPQ(10.0 / MIDI_RESOLUTION);
inline const PPQ MIDI_TICK_SIXTEENTH = PPQ(120.0 / MIDI_RESOLUTION);
inline const PPQ MIDI_TICK_SIXTEENTH_DOT = PPQ(160.0 / MIDI_RESOLUTION);
inline const PPQ MIDI_TICK_170 = PPQ(170.0 / MIDI_RESOLUTION);
inline const PPQ MIDI_TICK_EIGHTH = PPQ(240.0 / MIDI_RESOLUTION);

constexpr uint MIDI_MAX_MESSAGES_PER_BLOCK = 256;

inline const PPQ MIDI_CHORD_TOLERANCE = MIDI_TICK_BASE;

inline const PPQ MIDI_HOPO_SIXTEENTH = MIDI_TICK_SIXTEENTH;
inline const PPQ MIDI_HOPO_SIXTEENTH_DOT = MIDI_TICK_SIXTEENTH_DOT;
inline const PPQ MIDI_HOPO_CLASSIC_170 = MIDI_TICK_170;
inline const PPQ MIDI_HOPO_EIGHTH = MIDI_TICK_EIGHTH;

constexpr uint MIDI_LANE_COUNT = 7;

inline const PPQ MIDI_MIN_SUSTAIN_LENGTH = PPQ(4.0 / 12.0);
inline const PPQ MIDI_LANE_EXTENSION_TIME = PPQ(0.1);

inline const PPQ MIDI_HOPO_THRESHOLD_BUFFER = PPQ(0.01);

constexpr uint MIDI_PITCH_MIN = 0;
constexpr uint MIDI_PITCH_MAX = 127;
constexpr uint MIDI_PITCH_COUNT = 128;

constexpr uint DEBUG_FAKE_TRACK_WINDOW_NOTE_COUNT = 10;
