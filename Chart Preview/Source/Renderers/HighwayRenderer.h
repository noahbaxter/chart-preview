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

        void paint(juce::Graphics &g, PPQ trackWindowStartPPQ, PPQ trackWindowEndPPQ, PPQ displaySizeInPPQ);

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
        void drawGridline(juce::Graphics &g, float position, juce::Image *markerImage);

        void drawNotesFromMap(juce::Graphics &g, const TrackWindow& trackWindow, PPQ trackWindowStartPPQ, PPQ displaySizeInPPQ);
        void drawFrame(const std::array<Gem, LANE_COUNT> &gems, float position, PPQ framePosition);
        void drawGem(uint gemColumn, Gem gem, float position, PPQ framePosition);
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

        

};