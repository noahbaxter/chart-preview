/*
    ==============================================================================

        AssetManager.h
        Created: 15 Jun 2024 3:57:32pm
        Author:  Noah Baxter

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Utils.h"

// Forward declarations
class MidiInterpreter;

class AssetManager
{
public:
    AssetManager();
    ~AssetManager();

    // Image picker methods
    juce::Image* getGuitarGlyphImage(Gem gem, uint gemColumn, bool starPowerActive, bool spNoteHeld);
    juce::Image* getDrumGlyphImage(Gem gem, uint gemColumn, bool starPowerActive, bool spNoteHeld);
    juce::Image* getOverlayImage(Gem gem, Part part);

    // Bar/Open notes
    juce::Image* getBarKickImage() { return &barKickImage; }
    juce::Image* getBarKick2xImage() { return &barKick2xImage; }
    juce::Image* getBarOpenImage() { return &barOpenImage; }
    juce::Image* getBarWhiteImage() { return &barWhiteImage; }

    // Cymbal notes
    juce::Image* getCymBlueImage() { return &cymBlueImage; }
    juce::Image* getCymGreenImage() { return &cymGreenImage; }
    juce::Image* getCymRedImage() { return &cymRedImage; }
    juce::Image* getCymWhiteImage() { return &cymWhiteImage; }
    juce::Image* getCymYellowImage() { return &cymYellowImage; }

    // HOPO notes
    juce::Image* getHopoBlueImage() { return &hopoBlueImage; }
    juce::Image* getHopoGreenImage() { return &hopoGreenImage; }
    juce::Image* getHopoOrangeImage() { return &hopoOrangeImage; }
    juce::Image* getHopoRedImage() { return &hopoRedImage; }
    juce::Image* getHopoWhiteImage() { return &hopoWhiteImage; }
    juce::Image* getHopoYellowImage() { return &hopoYellowImage; }

    // Lane graphics
    juce::Image* getLaneEndImage() { return &laneEndImage; }
    juce::Image* getLaneMidImage() { return &laneMidImage; }
    juce::Image* getLaneStartImage() { return &laneStartImage; }

    // Marker graphics
    juce::Image* getMarkerBeatImage() { return &markerBeatImage; }
    juce::Image* getMarkerHalfBeatImage() { return &markerHalfBeatImage; }
    juce::Image* getMarkerMeasureImage() { return &markerMeasureImage; }

    // Regular notes
    juce::Image* getNoteBlueImage() { return &noteBlueImage; }
    juce::Image* getNoteGreenImage() { return &noteGreenImage; }
    juce::Image* getNoteOrangeImage() { return &noteOrangeImage; }
    juce::Image* getNoteRedImage() { return &noteRedImage; }
    juce::Image* getNoteWhiteImage() { return &noteWhiteImage; }
    juce::Image* getNoteYellowImage() { return &noteYellowImage; }

    // Overlay graphics
    juce::Image* getOverlayCymAccentImage() { return &overlayCymAccentImage; }
    juce::Image* getOverlayCymGhost80scaleImage() { return &overlayCymGhost80scaleImage; }
    juce::Image* getOverlayCymGhostImage() { return &overlayCymGhostImage; }
    juce::Image* getOverlayNoteAccentImage() { return &overlayNoteAccentImage; }
    juce::Image* getOverlayNoteGhostImage() { return &overlayNoteGhostImage; }
    juce::Image* getOverlayNoteTapImage() { return &overlayNoteTapImage; }

    // Sustain graphics
    juce::Image* getSustainBlueImage() { return &sustainBlueImage; }
    juce::Image* getSustainGreenImage() { return &sustainGreenImage; }
    juce::Image* getSustainOpenWhiteImage() { return &sustainOpenWhiteImage; }
    juce::Image* getSustainOpenImage() { return &sustainOpenImage; }
    juce::Image* getSustainOrangeImage() { return &sustainOrangeImage; }
    juce::Image* getSustainRedImage() { return &sustainRedImage; }
    juce::Image* getSustainWhiteImage() { return &sustainWhiteImage; }
    juce::Image* getSustainYellowImage() { return &sustainYellowImage; }

private:
    void initAssets();

    // Bar/Open notes
    juce::Image barKickImage;
    juce::Image barKick2xImage;
    juce::Image barOpenImage;
    juce::Image barWhiteImage;

    // Cymbal notes
    juce::Image cymBlueImage;
    juce::Image cymGreenImage;
    juce::Image cymRedImage;
    juce::Image cymWhiteImage;
    juce::Image cymYellowImage;

    // HOPO notes
    juce::Image hopoBlueImage;
    juce::Image hopoGreenImage;
    juce::Image hopoOrangeImage;
    juce::Image hopoRedImage;
    juce::Image hopoWhiteImage;
    juce::Image hopoYellowImage;

    // Lane graphics
    juce::Image laneEndImage;
    juce::Image laneMidImage;
    juce::Image laneStartImage;

    // Marker graphics
    juce::Image markerBeatImage;
    juce::Image markerHalfBeatImage;
    juce::Image markerMeasureImage;

    // Regular notes
    juce::Image noteBlueImage;
    juce::Image noteGreenImage;
    juce::Image noteOrangeImage;
    juce::Image noteRedImage;
    juce::Image noteWhiteImage;
    juce::Image noteYellowImage;

    // Overlay graphics
    juce::Image overlayCymAccentImage;
    juce::Image overlayCymGhost80scaleImage;
    juce::Image overlayCymGhostImage;
    juce::Image overlayNoteAccentImage;
    juce::Image overlayNoteGhostImage;
    juce::Image overlayNoteTapImage;

    // Sustain graphics
    juce::Image sustainBlueImage;
    juce::Image sustainGreenImage;
    juce::Image sustainOpenWhiteImage;
    juce::Image sustainOpenImage;
    juce::Image sustainOrangeImage;
    juce::Image sustainRedImage;
    juce::Image sustainWhiteImage;
    juce::Image sustainYellowImage;
};
