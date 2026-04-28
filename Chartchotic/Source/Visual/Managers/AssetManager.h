/*
    ==============================================================================

        AssetManager.h
        Created: 15 Jun 2024 3:57:32pm
        Author:  Noah Baxter

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Utils/ChartTypes.h"

class AssetManager
{
public:
    AssetManager();
    ~AssetManager();

    // Pre-scale all drawable assets to match the current viewport width.
    // Call on window resize. Skips work if width hasn't changed.
    void rescaleForWidth(int viewportWidth);

    // Image picker methods
    juce::Image* getGuitarGlyphImage(const GemWrapper& gem, uint gemColumn, bool starPowerActive);
    juce::Image* getDrumGlyphImage(const GemWrapper& gem, uint gemColumn, bool starPowerActive);
    juce::Image* getGridlineImage(Gridline gridlineType);
    juce::Image* getOverlayImage(Gem gem, Part part);
    juce::Colour getLaneColour(uint gemColumn, Part part, bool starPowerActive);

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
    juce::Image* getHopoBarOpenImage() { return &barWhiteImage; }   // TODO: create unique asset
    juce::Image* getHopoBarWhiteImage() { return &barWhiteImage; }  // TODO: create unique asset
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
    juce::Image* getMarkerStepImage() { return &markerStepImage; }

    // Regular notes
    juce::Image* getNoteBlueImage() { return &noteBlueImage; }
    juce::Image* getNoteGreenImage() { return &noteGreenImage; }
    juce::Image* getNoteOrangeImage() { return &noteOrangeImage; }
    juce::Image* getNoteRedImage() { return &noteRedImage; }
    juce::Image* getNoteWhiteImage() { return &noteWhiteImage; }
    juce::Image* getNoteYellowImage() { return &noteYellowImage; }

    // Overlay graphics
    juce::Image* getOverlayCymAccentImage() { return &overlayCymAccentImage; }
    juce::Image* getOverlayCymGhostImage() { return &overlayCymGhostImage; }
    juce::Image* getOverlayNoteAccentImage() { return &overlayNoteAccentImage; }
    juce::Image* getOverlayNoteGhostImage() { return &overlayNoteGhostImage; }
    juce::Image* getOverlayNoteTapImage() { return &overlayNoteTapImage; }

    // Sustain graphics (open only — colored sustains drawn flat by SustainRenderer)
    juce::Image* getSustainOpenWhiteImage() { return &sustainOpenWhiteImage; }
    juce::Image* getSustainOpenImage() { return &sustainOpenImage; }

    // Indicator graphics
    juce::Image* getDiscoBallImage() { return &discoBallImage; }

    // Hit animation graphics
    juce::Image* getHitAnimationFrame(int frameNumber) {
        if (frameNumber >= 1 && frameNumber <= 5) return &hitAnimationFrames[frameNumber - 1];
        return nullptr;
    }
    juce::Image* getHitFlareWhiteImage() { return &hitFlareImages[5]; }
    juce::Image* getHitFlarePurpleImage() { return &hitFlarePurpleImage; }
    juce::Image* getHitFlareImage(uint gemColumn, Part part) {
        // Map gemColumn to flare index based on color
        // Flare array: [0]=green, [1]=red, [2]=yellow, [3]=blue, [4]=orange
        // Guitar: 1=green, 2=red, 3=yellow, 4=blue, 5=orange -> direct mapping (column-1)
        // Drums:  1=red, 2=yellow, 3=blue, 4=green -> needs remapping
        if (part == Part::GUITAR && gemColumn >= 1 && gemColumn <= 5) {
            return &hitFlareImages[gemColumn - 1];
        } else if (part == Part::DRUMS && gemColumn >= 1 && gemColumn <= 4) {
            // Drum mapping: 1=red->1, 2=yellow->2, 3=blue->3, 4=green->0
            uint flareIndex = (gemColumn == 4) ? 0 : gemColumn;
            return &hitFlareImages[flareIndex];
        }
        return nullptr;
    }
    juce::Image* getKickAnimationFrame(int frameNumber) {
        if (frameNumber >= 1 && frameNumber <= 7) return &kickAnimationFrames[frameNumber - 1];
        return nullptr;
    }
    juce::Image* getOpenAnimationFrame(int frameNumber) {
        if (frameNumber >= 1 && frameNumber <= 7) return &openAnimationFrames[frameNumber - 1];
        return nullptr;
    }

private:
    void initAssets();

    // Downscale helper: returns src unchanged if already smaller than targetWidth
    static juce::Image downscale(const juce::Image& src, int targetWidth);

    // Full-resolution originals (kept for re-scaling on window resize)
    struct ScalableAsset
    {
        juce::Image* target;   // pointer to the active (scaled) member
        juce::Image fullRes;   // original full-res copy
    };
    std::vector<ScalableAsset> scalableAssets;
    int lastScaledWidth = 0;

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
    juce::Image markerStepImage;   // Generated programmatically (write-mode STEP overlay)

    // Regular notes
    juce::Image noteBlueImage;
    juce::Image noteGreenImage;
    juce::Image noteOrangeImage;
    juce::Image noteRedImage;
    juce::Image noteWhiteImage;
    juce::Image noteYellowImage;

    // Overlay graphics
    juce::Image overlayCymAccentImage;
    juce::Image overlayCymGhostImage;
    juce::Image overlayNoteAccentImage;
    juce::Image overlayNoteGhostImage;
    juce::Image overlayNoteTapImage;

    // Sustain graphics (open only)
    juce::Image sustainOpenWhiteImage;
    juce::Image sustainOpenImage;

    // Indicator graphics
    juce::Image discoBallImage;

    // Hit animation graphics
    juce::Image hitAnimationFrames[5];   // hit_1.png through hit_5.png
    juce::Image hitFlareImages[6];       // [0-4]=green/red/yellow/blue/orange, [5]=white
    juce::Image hitFlarePurpleImage;     // Generated at runtime by tinting white flare
    juce::Image kickAnimationFrames[7];  // hit_kick_1.png through hit_kick_7.png
    juce::Image openAnimationFrames[7];  // hit_open_1.png through hit_open_7.png
};
