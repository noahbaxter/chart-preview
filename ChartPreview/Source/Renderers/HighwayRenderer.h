/*
    ==============================================================================

        HighwayRenderer.h
        Created: 15 Jun 2024 3:57:32pm
        Author:  Noah Baxter

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Midi/MidiInterpreter.h"
#include "../Utils/Utils.h"
#include "AssetManager.h"


class HighwayRenderer
{
    public:
        HighwayRenderer(juce::ValueTree &state, MidiInterpreter &midiInterpreter);
        ~HighwayRenderer();

        void paint(juce::Graphics &g, PPQ trackWindowStartPPQ, PPQ trackWindowEndPPQ, PPQ displaySizeInPPQ, PPQ latencyBufferEnd);

    private:
        juce::ValueTree &state;
        MidiInterpreter &midiInterpreter;
        AssetManager assetManager;

        uint width = 0, height = 0;

        bool isBarNote(uint gemColumn, Part part)
        {
            if (part == Part::GUITAR)
            {
                return gemColumn == 0;
            }
            else // if (part == Part::DRUMS)
            {
                return gemColumn == 0 || gemColumn == 6;
            }
        }

        float calculateOpacity(float position)
        {
            // Make the gem fade out as it gets closer to the end
            if (position >= OPACITY_FADE_START)
            {
                return 1.0 - ((position - OPACITY_FADE_START) / (1.0f - OPACITY_FADE_START));
            }

            return 1.0;
        }

        DrawCallMap drawCallMap;
        void drawGridlinesFromMap(juce::Graphics &g, PPQ trackWindowStartPPQ, PPQ trackWindowEndPPQ, PPQ displaySizeInPPQ);
        void drawGridline(juce::Graphics &g, float position, juce::Image *markerImage, Gridline gridlineType);

        void drawNotesFromMap(juce::Graphics &g, const TrackWindow& trackWindow, PPQ trackWindowStartPPQ, PPQ displaySizeInPPQ);
        void drawFrame(const std::array<Gem, LANE_COUNT> &gems, float position, PPQ framePosition);
        void drawGem(uint gemColumn, Gem gem, float position, PPQ framePosition);
        
        void drawSustainFromWindow(juce::Graphics &g, const SustainWindow& sustainWindow, PPQ trackWindowStartPPQ, PPQ displaySizeInPPQ);
        void drawSustain(const SustainEvent& sustain, PPQ trackWindowStartPPQ, PPQ displaySizeInPPQ);
        juce::Rectangle<float> getSustainRect(uint gemColumn, float startPosition, float endPosition);
        void drawPerspectiveSustainFlat(juce::Graphics &g, uint gemColumn, float startPosition, float endPosition, float opacity, float sustainWidth, juce::Colour colour);
        std::pair<juce::Rectangle<float>, juce::Rectangle<float>> getSustainPositionRects(uint gemColumn, float startPosition, float endPosition);
        void draw(juce::Graphics &g, juce::Image *image, juce::Rectangle<float> position, float opacity)
        {
            g.setOpacity(opacity);
            g.drawImage(*image, position);
        };

        juce::Rectangle<float> getGuitarGlyphRect(uint gemColumn, float position);
        juce::Rectangle<float> getDrumGlyphRect(uint gemColumn, float position);
        juce::Rectangle<float> getOverlayGlyphRect(Gem gem, juce::Rectangle<float> glyphRect);
        juce::Rectangle<float> createPerspectiveGlyphRect(
            float position,
            float normY1, float normY2,
            float normX1, float normX2,
            float normWidth1, float normWidth2,
            bool isBarNote
        );

        // Testing helper functions
        TrackWindow generateFakeTrackWindow(PPQ trackWindowStartPPQ, PPQ trackWindowEndPPQ)
        {
            TrackWindow fakeTrackWindow;

            // Use PPQ values, not floats - create notes every 0.25 PPQ starting from trackWindowStartPPQ
            fakeTrackWindow[trackWindowStartPPQ + PPQ(0.00)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(0.25)] = {Gem::NOTE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(0.50)] = {Gem::NONE, Gem::NOTE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(0.75)] = {Gem::NONE, Gem::NONE, Gem::NOTE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(1.00)] = {Gem::NONE, Gem::NONE, Gem::NONE, Gem::NOTE, Gem::NONE, Gem::NONE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(1.25)] = {Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NOTE, Gem::NONE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(1.50)] = {Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(1.75)] = {Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NOTE};

            return fakeTrackWindow;
        }

        TrackWindow generateFullFakeTrackWindow(PPQ trackWindowStartPPQ, PPQ trackWindowEndPPQ)
        {
            TrackWindow fakeTrackWindow;

            // Use PPQ values, not floats - create notes every 0.25 PPQ starting from trackWindowStartPPQ
            fakeTrackWindow[trackWindowStartPPQ + PPQ(0.00)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(0.25)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(0.50)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(0.75)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(1.00)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(1.25)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(1.50)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(1.75)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(2.00)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(2.25)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(2.50)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(2.75)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(3.00)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(3.25)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(3.50)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(3.75)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};
            fakeTrackWindow[trackWindowStartPPQ + PPQ(4.00)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE};

            return fakeTrackWindow;
        }

        SustainWindow generateFakeSustainWindow(PPQ trackWindowStartPPQ, PPQ trackWindowEndPPQ)
        {
            SustainWindow fakeSustainWindow;

            if (isPart(state, Part::GUITAR)) {
                // Guitar uses lanes 0-5 (6 lanes total)
                for (uint i = 0; i < 6; i++)
                {
                    SustainEvent sustain;
                    sustain.startPPQ = trackWindowStartPPQ + PPQ(0.0);
                    sustain.endPPQ = trackWindowStartPPQ + PPQ(2.0);
                    sustain.gemColumn = i;
                    sustain.gemType = Gem::NOTE;
                    fakeSustainWindow.push_back(sustain);
                }
            } else {
                // Drums uses lanes 0,1,2,3,4 (5 lanes total)
                for (uint i = 0; i < 5; i++)
                {
                    SustainEvent sustain;
                    sustain.startPPQ = trackWindowStartPPQ + PPQ(0.0);
                    sustain.endPPQ = trackWindowStartPPQ + PPQ(2.0);
                    sustain.gemColumn = i;
                    sustain.gemType = Gem::NOTE;
                    fakeSustainWindow.push_back(sustain);
                }
            }

            return fakeSustainWindow;
        }

};