#include "FrameDataBuilder.h"
#include "../PluginProcessor.h"
#include "../Midi/Pipelines/ReaperMidiPipeline.h"
#include "../Midi/Utils/MidiConstants.h"
#include "../UI/ControlConstants.h"
#include "../Midi/Utils/TempoTimeSignatureEventHelper.h"
#include "../Visual/Utils/PositionMath.h"

// Apply latency offset to raw cursor position.
// In REAPER mode, uses the tempo map for accurate time-to-PPQ conversion
// so the offset stays consistent through tempo changes.
static PPQ applyLatencyOffset(PPQ cursorPPQ, const FrameContext& ctx)
{
    int latencyOffsetMs = (int)ctx.state.getProperty("latencyOffsetMs");
    if (latencyOffsetMs == 0) return cursorPPQ;

    auto& reaperProvider = ctx.processor.getReaperMidiProvider();
    if (reaperProvider.isReaperApiAvailable())
    {
        double cursorTime = reaperProvider.ppqToTime(cursorPPQ.toDouble());
        double offsetTime = cursorTime - (latencyOffsetMs / 1000.0);
        return PPQ(reaperProvider.timeToPpq(offsetTime));
    }

    // Non-REAPER fallback: use instantaneous BPM (no tempo map available)
    if (auto* playHead = ctx.processor.getPlayHead())
    {
        auto positionInfo = playHead->getPosition();
        if (positionInfo.hasValue())
        {
            double bpm = positionInfo->getBpm().orFallback(120.0);
            double latencyOffsetBeats = (latencyOffsetMs / 1000.0) * (bpm / 60.0);
            cursorPPQ = cursorPPQ - PPQ(latencyOffsetBeats);
        }
    }
    return cursorPPQ;
}

void FrameDataBuilder::buildReaper(HighwayFrameData& out,
                                   const FrameContext& ctx,
                                   MidiInterpreter& interpreter)
{
    PPQ trackWindowStartPPQ = applyLatencyOffset(ctx.cursorPPQ, ctx);
    PPQ trackWindowEndPPQ = trackWindowStartPPQ + ctx.displaySizeInPPQ;
    PPQ latencyBufferEnd = trackWindowStartPPQ;
    PPQ extendedStart = trackWindowStartPPQ - ctx.displaySizeInPPQ;

    // Wire disco flip state — prefer InstrumentSession (multi-track), fall back to pipeline (single-track)
    const DiscoFlipState* flipState = nullptr;
    auto* reaperPipeline = dynamic_cast<ReaperMidiPipeline*>(ctx.processor.getMidiPipeline());
    if (!ctx.processor.getInstrumentSession())
    {
        if (reaperPipeline)
            flipState = reaperPipeline->getDiscoFlipState();
        interpreter.setDiscoFlipState(flipState);
    }
    else
    {
        flipState = interpreter.getDiscoFlipState();
    }

    // Resolve all 4 difficulties in one lock — extract the active one
    PartWindow partWindow = interpreter.resolveAllDifficulties(extendedStart, trackWindowEndPPQ, latencyBufferEnd);
    SkillLevel activeSkill = (SkillLevel)((int)interpreter.getState().getProperty("skillLevel"));
    auto& diffWindow = partWindow.forSkill(activeSkill);

    auto& reaperProvider = ctx.processor.getReaperMidiProvider();
    auto ppqToTime = [&reaperProvider](double ppq) { return reaperProvider.ppqToTime(ppq); };

    TempoTimeSignatureMap tempoTimeSigMap;
    {
        const juce::ScopedLock lock(ctx.processor.getTempoLock());
        tempoTimeSigMap = ctx.processor.getTempoTimeSignatureMap();
    }

    PPQ cursorPPQ = trackWindowStartPPQ;
    out.trackWindow = TimeConverter::convertTrackWindow(diffWindow.trackWindow, cursorPPQ, ppqToTime);
    out.sustainWindow = TimeConverter::convertSustainWindow(diffWindow.sustainWindow, cursorPPQ, ppqToTime);
    out.gridlines = GridlineGenerator::generateGridlines(tempoTimeSigMap, extendedStart, trackWindowEndPPQ, cursorPPQ, ppqToTime, ctx.writeGridConfig);
    out.eventMarkers = TempoTimeSignatureEventHelper::buildTempoEventMarkers(tempoTimeSigMap, cursorPPQ, ppqToTime);
    out.windowStartTime = 0.0;
    out.windowEndTime = ctx.displayWindowTimeSeconds;
    out.isPlaying = ctx.processor.isPlaying;
    out.scrollOffset = ctx.scrollOffset;

    // Disco flip indicator + region markers
    {
        const auto& interpState = interpreter.getState();
        bool isProDrums = (int)interpState.getProperty("drumType") == 2;
        bool discoEnabled = (bool)interpState.getProperty("discoFlip");
        int midiDiff = (int)interpState.getProperty("skillLevel") - 1;
        out.discoFlipActive = flipState != nullptr && isProDrums && discoEnabled
                              && flipState->isFlipped(trackWindowStartPPQ, midiDiff);

        if (flipState != nullptr && isProDrums && discoEnabled)
        {
            double cursorTime = ppqToTime(cursorPPQ.toDouble());
            for (const auto& r : flipState->getRegions(midiDiff))
            {
                double startTime = ppqToTime(r.start.toDouble()) - cursorTime;
                double endTime   = ppqToTime(r.end.toDouble())   - cursorTime;
                if (startTime > 0.0 || endTime > -ctx.displayWindowTimeSeconds)
                    out.flipRegions.push_back({startTime, endTime});
            }
        }
    }
}

void FrameDataBuilder::buildReaperBatched(HighwayFrameData& primaryOut,
                                          const FrameContext& ctx)
{
    PPQ trackWindowStartPPQ = applyLatencyOffset(ctx.cursorPPQ, ctx);
    PPQ trackWindowEndPPQ = trackWindowStartPPQ + ctx.displaySizeInPPQ;
    PPQ latencyBufferEnd = trackWindowStartPPQ;
    PPQ extendedStart = trackWindowStartPPQ - ctx.displaySizeInPPQ;
    PPQ cursorPPQ = trackWindowStartPPQ;

    auto& reaperProvider = ctx.processor.getReaperMidiProvider();
    auto ppqToTime = [&reaperProvider](double ppq) { return reaperProvider.ppqToTime(ppq); };

    TempoTimeSignatureMap tempoTimeSigMap;
    {
        const juce::ScopedLock lock(ctx.processor.getTempoLock());
        tempoTimeSigMap = ctx.processor.getTempoTimeSignatureMap();
    }

    TimeBasedGridlineMap sharedGridlines = GridlineGenerator::generateGridlines(
        tempoTimeSigMap, extendedStart, trackWindowEndPPQ, cursorPPQ, ppqToTime, ctx.writeGridConfig);
    TimeBasedEventMarkers sharedMarkers = TempoTimeSignatureEventHelper::buildTempoEventMarkers(
        tempoTimeSigMap, cursorPPQ, ppqToTime);

    bool isPlaying = ctx.processor.isPlaying;

    // Group slots by lock pointer
    std::unordered_map<juce::CriticalSection*, std::vector<int>> lockGroups;
    for (int i = 0; i < ctx.activeSlotCount; i++)
        lockGroups[&ctx.slots[i].interpreter->noteStateMapLock].push_back(i);

    for (auto& [lockPtr, slotIndices] : lockGroups)
    {
        auto& firstInterp = *ctx.slots[slotIndices[0]].interpreter;

        TrackResolver::Config cfg;
        cfg.part = firstInterp.instrumentPart;
        cfg.autoHopo = (bool)firstInterp.getState().getProperty("autoHopo", false);
        cfg.dynamics = (bool)firstInterp.getState().getProperty("dynamics");
        cfg.proDrums = (DrumType)((int)firstInterp.getState().getProperty("drumType")) == DrumType::PRO;
        cfg.kick2x = (bool)firstInterp.getState().getProperty("kick2x");
        cfg.discoFlip = (bool)firstInterp.getState().getProperty("discoFlip");
        cfg.starPower = (bool)firstInterp.getState().getProperty("starPower");
        cfg.bemaniMode = PositionMath::bemaniMode;
        cfg.discoFlipState = firstInterp.getDiscoFlipState();

        int thresholdIndex = (int)firstInterp.getState().getProperty("hopoThresh", HOPO_THRESHOLD_DEFAULT);
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

        SharedWindow shared;
        {
            const juce::ScopedLock lock(*lockPtr);
            shared = TrackResolver::extract(firstInterp.noteStateMapArray,
                                            extendedStart, trackWindowEndPPQ, latencyBufferEnd,
                                            cfg.bemaniMode);
        }

        PartWindow partWindow = TrackResolver::resolve(shared, cfg);

        for (int idx : slotIndices)
        {
            auto& slot = ctx.slots[idx];
            auto& interp = *slot.interpreter;

            SkillLevel activeSkill = (SkillLevel)((int)interp.getState().getProperty("skillLevel"));
            auto& diffWindow = partWindow.forSkill(activeSkill);

            HighwayFrameData frameData;
            frameData.trackWindow = TimeConverter::convertTrackWindow(diffWindow.trackWindow, cursorPPQ, ppqToTime);
            frameData.sustainWindow = TimeConverter::convertSustainWindow(diffWindow.sustainWindow, cursorPPQ, ppqToTime);
            frameData.gridlines = sharedGridlines;
            frameData.eventMarkers = sharedMarkers;
            frameData.windowStartTime = 0.0;
            frameData.windowEndTime = ctx.displayWindowTimeSeconds;
            frameData.isPlaying = isPlaying;
            frameData.scrollOffset = ctx.scrollOffset;

            // Disco flip per-slot
            {
                const DiscoFlipState* flipState = interp.getDiscoFlipState();
                const auto& interpState = interp.getState();
                bool isProDrums = (int)interpState.getProperty("drumType") == 2;
                bool discoEnabled = (bool)interpState.getProperty("discoFlip");
                int midiDiff = (int)interpState.getProperty("skillLevel") - 1;
                frameData.discoFlipActive = flipState != nullptr && isProDrums && discoEnabled
                                            && flipState->isFlipped(trackWindowStartPPQ, midiDiff);

                if (flipState != nullptr && isProDrums && discoEnabled)
                {
                    double cursorTime = ppqToTime(cursorPPQ.toDouble());
                    for (const auto& r : flipState->getRegions(midiDiff))
                    {
                        double startTime = ppqToTime(r.start.toDouble()) - cursorTime;
                        double endTime   = ppqToTime(r.end.toDouble())   - cursorTime;
                        if (startTime > 0.0 || endTime > -ctx.displayWindowTimeSeconds)
                            frameData.flipRegions.push_back({startTime, endTime});
                    }
                }
            }

            frameData.builtForPart = interp.instrumentPart;
            if (idx == 0)
                primaryOut = frameData;
            slot.highway->setFrameData(frameData);
            slot.highway->repaint();
        }
    }
}

void FrameDataBuilder::buildStandard(HighwayFrameData& out,
                                     const FrameContext& ctx,
                                     MidiInterpreter& interpreter)
{
    PPQ trackWindowStartPPQ = ctx.cursorPPQ;

    // Apply latency compensation when playing
    if (ctx.processor.isPlaying)
    {
        PPQ smoothedLatency = ctx.smoothedLatencyInPPQ;
        trackWindowStartPPQ = std::max(PPQ(0.0), trackWindowStartPPQ - smoothedLatency);
    }

    // Apply latency offset (uses tempo map in REAPER mode, instantaneous BPM otherwise)
    trackWindowStartPPQ = applyLatencyOffset(trackWindowStartPPQ, ctx);

    PPQ trackWindowEndPPQ = trackWindowStartPPQ + ctx.displaySizeInPPQ;
    PPQ latencyBufferEnd = trackWindowStartPPQ + ctx.smoothedLatencyInPPQ;

    ctx.processor.setMidiProcessorVisualWindowBounds(trackWindowStartPPQ, trackWindowEndPPQ);

    double currentBPM = 120.0;
    auto* playHead = ctx.processor.getPlayHead();
    if (playHead)
    {
        auto positionInfo = playHead->getPosition();
        if (positionInfo.hasValue())
            currentBPM = positionInfo->getBpm().orFallback(120.0);
    }

    PPQ extendedStart = trackWindowStartPPQ - ctx.displaySizeInPPQ;

    PartWindow partWindow = interpreter.resolveAllDifficulties(extendedStart, trackWindowEndPPQ, latencyBufferEnd);
    SkillLevel activeSkill = (SkillLevel)((int)interpreter.getState().getProperty("skillLevel"));
    auto& diffWindow = partWindow.forSkill(activeSkill);

    auto ppqToTime = [currentBPM](double ppq) { return ppq * (60.0 / currentBPM); };

    TempoTimeSignatureMap tempoTimeSigMap;
    {
        const juce::ScopedLock lock(ctx.processor.getTempoLock());
        tempoTimeSigMap = ctx.processor.getTempoTimeSignatureMap();
        if (tempoTimeSigMap.empty())
            tempoTimeSigMap[PPQ(0.0)] = TempoTimeSignatureEvent(PPQ(0.0), currentBPM, 4, 4);
    }

    PPQ cursorPPQ = trackWindowStartPPQ;
    out.trackWindow = TimeConverter::convertTrackWindow(diffWindow.trackWindow, cursorPPQ, ppqToTime);
    out.sustainWindow = TimeConverter::convertSustainWindow(diffWindow.sustainWindow, cursorPPQ, ppqToTime);
    out.gridlines = GridlineGenerator::generateGridlines(tempoTimeSigMap, extendedStart, trackWindowEndPPQ, cursorPPQ, ppqToTime, ctx.writeGridConfig);
    out.eventMarkers = TempoTimeSignatureEventHelper::buildTempoEventMarkers(tempoTimeSigMap, cursorPPQ, ppqToTime);
    out.windowStartTime = 0.0;
    out.windowEndTime = ctx.displayWindowTimeSeconds;
    out.isPlaying = ctx.processor.isPlaying;
    out.scrollOffset = ctx.scrollOffset;
    out.builtForPart = interpreter.instrumentPart;
}
