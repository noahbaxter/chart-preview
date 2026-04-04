#pragma once

#include <JuceHeader.h>

class ChartchoticAudioProcessor;
class HighwayComponent;
class MidiWriter;

struct WriteSelection
{
    double ppq = -1.0;
    int pitch = -1;
    int lane = -1;

    bool hasSelection() const { return ppq >= 0.0 && lane >= 0; }
    static WriteSelection none() { return {}; }
};

class WriteController
{
public:
    WriteController() = default;
    ~WriteController();

    void init(ChartchoticAudioProcessor& processor,
              juce::ValueTree& state,
              HighwayComponent& highway);

    void toggle();
    void update(bool isPlaying);

    bool isActive() const { return active; }
    const WriteSelection& getSelection() const { return selection; }

private:
    ChartchoticAudioProcessor* processor = nullptr;
    juce::ValueTree* state = nullptr;
    HighwayComponent* highway = nullptr;

    bool active = false;
    WriteSelection selection;
    bool wasPlaying = false;

    void wireCallbacks();
    void clearCallbacks();

    // Find note index in REAPER track matching timeFromCursor+pitch.
    // Returns index or -1. Sets outPPQ to the resolved PPQ position.
    int findNoteIndex(double timeFromCursor, int pitch, double& outPPQ);

    // Resolve lane index to pitch using current skill level and instrument
    void resolvePitches(std::vector<uint>& out);

    // Resolve lane to pitch (convenience for single lookup)
    int pitchForLane(int lane);

    // Set selection from timeFromCursor + lane (resolves PPQ internally)
    void selectFromHit(double timeFromCursor, int lane);
};
