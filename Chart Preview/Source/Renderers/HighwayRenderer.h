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
#include "../Utils.h"
#include "AssetManager.h"


class HighwayRenderer
{
    public:
        HighwayRenderer(juce::ValueTree &state, MidiInterpreter &midiInterpreter);
        ~HighwayRenderer();

        uint width = 0, height = 0;

        void paint(juce::Graphics &g, uint trackWindowStart, uint trackWindowEnd, uint displaySizeInSamples);


    private:
        juce::ValueTree &state;
        MidiInterpreter &midiInterpreter;
        AssetManager &assetManager;

        uint framePosition = 0;

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
            float opacityStart = 0.9;
            if (position >= opacityStart)
            {
                return 1.0 - ((position - opacityStart) / (1.0f - opacityStart));
            }

            return 1.0;
        }

        DrawCallMap drawCallMap;
        void drawFrame(const std::array<Gem, 7> &gems, float position);
        void drawGem(uint gemColumn, Gem gem, float position);
        void draw(juce::Graphics &g, juce::Image *image, juce::Rectangle<float> position, float opacity)
        {
            g.setOpacity(opacity);
            g.drawImage(*image, position);
        };

        juce::Rectangle<float> createGlyphRect(float position, 
                                               float normY1, float normY2, 
                                               float normX1, float normX2, 
                                               float normWidth1, float normWidth2, 
                                               bool isBarNote);
        juce::Rectangle<float> getGuitarGlyphRect(uint gemColumn, float position);
        juce::Rectangle<float> getDrumGlyphRect(uint gemColumn, float position);
        juce::Rectangle<float> getOverlayGlyphRect(Gem gem, juce::Rectangle<float> glyphRect);



};