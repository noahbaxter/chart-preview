/*
  ==============================================================================

    MidiInterpreter.cpp
    Created: 15 Jun 2024 3:57:52pm
    Author:  Noah Baxter

  ==============================================================================
*/

#include "MidiInterpreter.h"
#include "../Utils/MidiConstants.h"
#include "../../Visual/Geometry/PositionMath.h"

MidiInterpreter::MidiInterpreter(juce::ValueTree &state, NoteStateMapArray &noteStateMapArray, juce::CriticalSection &noteStateMapLock)
    : noteStateMapArray(noteStateMapArray),
      noteStateMapLock(noteStateMapLock),
      state(state)
{
    instrumentPart = getPartFromState(state);
}

MidiInterpreter::MidiInterpreter(Part part, juce::ValueTree &state, NoteStateMapArray &noteStateMapArray, juce::CriticalSection &noteStateMapLock)
    : instrumentPart(part),
      noteStateMapArray(noteStateMapArray),
      noteStateMapLock(noteStateMapLock),
      state(state)
{
}

MidiInterpreter::~MidiInterpreter()
{
}

//================================================================================
// resolveAllDifficulties — one lock, one pass, all 4 difficulties
//================================================================================

PartWindow MidiInterpreter::resolveAllDifficulties(PPQ windowStart, PPQ windowEnd, PPQ latencyEnd)
{
    // Build config from ValueTree (read once, pass as pure data)
    TrackResolver::Config cfg;
    cfg.part = instrumentPart;
    cfg.autoHopo = (bool)state.getProperty("autoHopo", false);
    cfg.dynamics = (bool)state.getProperty("dynamics");
    cfg.proDrums = (DrumType)((int)state.getProperty("drumType")) == DrumType::PRO;
    cfg.kick2x = (bool)state.getProperty("kick2x");
    cfg.discoFlip = (bool)state.getProperty("discoFlip");
    cfg.starPower = (bool)state.getProperty("starPower");
    cfg.bemaniMode = PositionMath::bemaniMode;
    cfg.discoFlipState = discoFlip;

    int thresholdIndex = (int)state.getProperty("hopoThresh", HOPO_THRESHOLD_DEFAULT);
    if (cfg.autoHopo)
    {
        switch (thresholdIndex)
        {
            case 0: cfg.hopoThreshold = MIDI_HOPO_SIXTEENTH;   break;
            case 1: cfg.hopoThreshold = MIDI_HOPO_CLASSIC_170;  break;
            case 2: cfg.hopoThreshold = MIDI_HOPO_EIGHTH;       break;
            default: cfg.hopoThreshold = MIDI_HOPO_CLASSIC_170; break;
        }
        cfg.hopoThreshold += MIDI_HOPO_THRESHOLD_BUFFER;
    }

    // Extract under one lock
    SharedWindow shared;
    {
        const juce::ScopedLock lock(noteStateMapLock);
        bool isElite = getRenderType(cfg.part) == RenderType::ELITE_DRUMS;
        shared = TrackResolver::extract(noteStateMapArray, windowStart, windowEnd, latencyEnd, cfg.bemaniMode, isElite);
    }

    // Resolve on local data — no locks
    return TrackResolver::resolve(shared, cfg);
}
