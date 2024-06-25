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


class HighwayRenderer
{
    public:
        HighwayRenderer(juce::ValueTree &state, MidiInterpreter &midiInterpreter);
        ~HighwayRenderer();

        uint width = 0, height = 0;

        void initAssets();
        void paint(juce::Graphics &g, uint trackWindowStart, uint trackWindowEnd, uint displaySizeInSamples);


    private:
        
        juce::ValueTree &state;
        MidiInterpreter &midiInterpreter;

        uint framePosition = 0;

        // Chart Assets
        juce::Image barKickImage;
        juce::Image barKick2xImage;
        juce::Image barOpenImage;
        juce::Image barWhiteImage;

        juce::Image cymBlueImage;
        juce::Image cymGreenImage;
        juce::Image cymRedImage;
        juce::Image cymWhiteImage;
        juce::Image cymYellowImage;

        juce::Image hopoBlueImage;
        juce::Image hopoGreenImage;
        juce::Image hopoOrangeImage;
        juce::Image hopoRedImage;
        juce::Image hopoWhiteImage;
        juce::Image hopoYellowImage;

        juce::Image laneEndImage;
        juce::Image laneMidImage;
        juce::Image laneStartImage;

        juce::Image noteBlueImage;
        juce::Image noteGreenImage;
        juce::Image noteOrangeImage;
        juce::Image noteRedImage;
        juce::Image noteWhiteImage;
        juce::Image noteYellowImage;

        juce::Image overlayCymAccentImage;
        juce::Image overlayCymGhost80scaleImage;
        juce::Image overlayCymGhostImage;
        juce::Image overlayNoteAccentImage;
        juce::Image overlayNoteGhostImage;
        juce::Image overlayNoteTapImage;

        juce::Image sustainBlueImage;
        juce::Image sustainGreenImage;
        juce::Image sustainOpenWhiteImage;
        juce::Image sustainOpenImage;
        juce::Image sustainOrangeImage;
        juce::Image sustainRedImage;
        juce::Image sustainWhiteImage;
        juce::Image sustainYellowImage;

        void drawFrame(juce::Graphics &g, const std::array<Gem, 7> &gems, float position);
        void drawGem(juce::Graphics &g, uint gemColumn, Gem gem, float position);

        juce::Rectangle<float> createGlyphRect(float position, 
                                               float normY1, float normY2, 
                                               float normX1, float normX2, 
                                               float normWidth1, float normWidth2, 
                                               bool isBarNote);
        juce::Rectangle<float> getGuitarGlyphRect(uint gemColumn, float position);
        juce::Rectangle<float> getDrumGlyphRect(uint gemColumn, float position);
        juce::Rectangle<float> getOverlayGlyphRect(Gem gem, juce::Rectangle<float> glyphRect);

        juce::Image getGuitarGlyphImage(Gem gem, uint gemColumn);
        juce::Image getDrumGlyphImage(Gem gem, uint gemColumn);
        juce::Image getOverlayImage(Gem gem);

        void fadeInImage(juce::Image &image, float position);
};