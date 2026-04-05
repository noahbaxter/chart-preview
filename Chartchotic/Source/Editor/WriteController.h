#pragma once

#include <JuceHeader.h>

class ChartchoticAudioProcessor;
class HighwayComponent;
class MidiWriter;

enum class InteractionMode { DRAW, EDIT };

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

    // Sub-mode
    void toggleMode();
    InteractionMode getMode() const { return mode; }

    // Grid config
    int getStepDivision() const { return stepDivision; }
    void setStepDivision(int div);
    void halveStepDivision();
    void doubleStepDivision();

    int getTuplet() const { return tuplet; }
    void cycleTuplet();

    bool isSnapEnabled() const { return snapEnabled; }
    void setSnapEnabled(bool on);

private:
    ChartchoticAudioProcessor* processor = nullptr;
    juce::ValueTree* state = nullptr;
    HighwayComponent* highway = nullptr;

    bool active = false;
    InteractionMode mode = InteractionMode::DRAW;
    WriteSelection selection;
    bool wasPlaying = false;

    // Grid state
    int stepDivision = 4;      // 1/N note (1,2,4,8,16,32,64)
    int tuplet = 0;            // 0=normal, 3=triplet, 5=quintuplet, 7=septuplet
    bool snapEnabled = true;

    void wireCallbacks();
    void clearCallbacks();

    // Common helpers
    int getTrackIndex() const;
    double timeFromCursorToPPQ(double timeFromCursor) const;

    int findNoteIndex(double timeFromCursor, int pitch, double& outPPQ);
    int findNoteIndexByPPQ(double ppq, int pitch);
    void resolvePitches(std::vector<uint>& out);
    int pitchForLane(int lane);
    void selectFromHit(double timeFromCursor, int lane);

    // Place a short note at the given position
    void placeNote(double timeFromCursor, int lane);
    // Erase the note at the given position
    void eraseNote(double timeFromCursor, int lane);

    // Find the PPQ of the next note with the same pitch after afterPPQ. Returns -1 if none.
    double findNextNotePPQ(double afterPPQ, int pitch);

    // Snap a PPQ position to the nearest grid line
    double snapToGrid(double ppq) const;

    // Snap to grid or nearest note in pitch (whichever is closer)
    double snapToGridOrNote(double ppq, int pitch);

    // PPQ distance for one step at current settings
    double stepSizeInPPQ() const;
};
