/*
    ==============================================================================

        AssetManager.h
        Created: 15 Jun 2024 3:57:32pm
        Author:  Noah Baxter

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <map>
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
    juce::Image* getDrumGlyphImage(const GemWrapper& gem, uint gemColumn, bool starPowerActive, bool elite = false);
    juce::Image* getGridlineImage(Gridline gridlineType);
    juce::Image* getOverlayImage(Gem gem, Part part);
    juce::Colour getLaneColour(uint gemColumn, Part part, bool starPowerActive);

    // Bar/Open notes
    juce::Image* getBarKickImage() { return &barKickImage; }
    juce::Image* getBarKick2xImage() { return &barKick2xImage; }
    juce::Image* getBarOpenImage() { return &barOpenImage; }
    juce::Image* getBarWhiteImage() { return &barWhiteImage; }
    juce::Image* getBarKickEliteImage()   { return &barKickEliteImage; }
    juce::Image* getBarKick2xEliteImage() { return &barKick2xEliteImage; }
    juce::Image* getBarWhiteEliteImage()  { return &barWhiteEliteImage; }
    juce::Image* getBarKickAccentImage()   { return &barKickAccentImage; }
    juce::Image* getBarKick2xAccentImage() { return &barKick2xAccentImage; }
    juce::Image* getBarWhiteAccentImage()  { return &barWhiteAccentImage; }
    // Elite hi-hat pedal bars, drawn on cols 10 / 11.
    juce::Image* getBarStompImage() { return &barStompImage; }
    juce::Image* getBarSplashImage() { return &barSplashImage; }

    // Cymbal notes
    juce::Image* getCymBlueImage() { return &cymBlueImage; }
    juce::Image* getCymGreenImage() { return &cymGreenImage; }
    juce::Image* getCymRedImage() { return &cymRedImage; }
    juce::Image* getCymWhiteImage() { return &cymWhiteImage; }
    juce::Image* getCymYellowImage() { return &cymYellowImage; }
    juce::Image* getCymPurpleImage() { return &cymPurpleImage; }
    // Elite hi-hat: extracted gold art used as-is (not tinted), replaces the yellow cym on the Hi-Hat lane.
    juce::Image* getCymHiHatClosedImage() { return &cymHiHatClosedImage; }
    juce::Image* getCymHiHatOpenImage() { return &cymHiHatOpenImage; }

    // HOPO notes
    juce::Image* getHopoBarOpenImage() { return &barWhiteImage; }   // TODO: create unique asset
    juce::Image* getHopoBarWhiteImage() { return &barWhiteImage; }  // TODO: create unique asset
    juce::Image* getHopoBlueImage() { return &hopoBlueImage; }
    juce::Image* getHopoGreenImage() { return &hopoGreenImage; }
    juce::Image* getHopoOrangeImage() { return &hopoOrangeImage; }
    juce::Image* getHopoRedImage() { return &hopoRedImage; }
    juce::Image* getHopoWhiteImage() { return &hopoWhiteImage; }
    juce::Image* getHopoYellowImage() { return &hopoYellowImage; }
    juce::Image* getHopoPurpleImage() { return &hopoPurpleImage; }

    // Lane graphics
    juce::Image* getLaneEndImage() { return &laneEndImage; }
    juce::Image* getLaneMidImage() { return &laneMidImage; }
    juce::Image* getLaneStartImage() { return &laneStartImage; }

    // Marker graphics
    juce::Image* getMarkerBeatImage() { return &markerBeatImage; }
    juce::Image* getMarkerHalfBeatImage() { return &markerHalfBeatImage; }
    juce::Image* getMarkerMeasureImage() { return &markerMeasureImage; }
    // Write-mode boosted variants — alpha amplified at load time so write
    // mode anchors render at full opacity through the same sprite path.
    juce::Image* getMarkerMeasureWriteImage() { return &markerMeasureWriteImage; }
    juce::Image* getMarkerBeatWriteImage()    { return &markerBeatWriteImage; }

    // Regular notes
    juce::Image* getNoteBlueImage() { return &noteBlueImage; }
    juce::Image* getNoteGreenImage() { return &noteGreenImage; }
    juce::Image* getNoteOrangeImage() { return &noteOrangeImage; }
    juce::Image* getNoteRedImage() { return &noteRedImage; }
    juce::Image* getNoteWhiteImage() { return &noteWhiteImage; }
    juce::Image* getNoteYellowImage() { return &noteYellowImage; }
    juce::Image* getNotePurpleImage() { return &notePurpleImage; }

    // Ghost cursor blanks (write-mode hover preview)
    juce::Image* getNoteBlankImage() { return &noteBlankImage; }
    juce::Image* getHopoBlankImage() { return &hopoBlankImage; }
    juce::Image* getCymBlankImage()  { return &cymBlankImage; }
    juce::Image* getBarBlankImage()  { return &barBlankImage; }
    juce::Image* getGhostCursorImage(bool isDrums, int lane);

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
    // Star-power flare is the untinted greyscale "smoke" master itself.
    juce::Image* getHitFlareWhiteImage() { return &hitFlareWhite; }
    // Guitar tap flare — the shared purple.
    juce::Image* getHitFlarePurpleImage();
    // Per-lane flare: tint the one greyscale smoke master by the lane's colour
    // (getLaneColour is part- and lane-count-generic), so any highway lights up
    // in its own colour with no per-lane art. Cached per colour.
    juce::Image* getHitFlareImage(uint gemColumn, Part part);
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

    // How wide an asset is ever actually drawn, as a fraction of the viewport. Scaling
    // everything to one size made every gem blit resample a source many times larger than
    // its destination, and JUCE's cost scales with the SOURCE, not the destination: on elite
    // that was ~230us per gem against ~36us from a lane-sized source. Bars really do span the
    // board, so they keep a full-width master and stay sharp.
    enum class AssetWidthClass
    {
        FullBoard,   // kick / open / star-power bars: drawn edge to edge
        Lane,        // gems, overlays, hit flashes: never wider than one lane's gem box
    };

    // Full-resolution originals (kept for re-scaling on window resize)
    struct ScalableAsset
    {
        juce::Image* target;   // pointer to the active (scaled) member
        juce::Image fullRes;   // original full-res copy
        AssetWidthClass widthClass = AssetWidthClass::Lane;
    };
    std::vector<ScalableAsset> scalableAssets;
    int lastScaledWidth = 0;

    // Bar/Open notes
    juce::Image barKickImage;
    juce::Image barKick2xImage;
    juce::Image barOpenImage;
    juce::Image barWhiteImage;
    juce::Image barKickEliteImage;      // Elite variants: thinner tube, same arc/centreline
    juce::Image barKick2xEliteImage;
    juce::Image barWhiteEliteImage;
    juce::Image barKickAccentImage;     // Elite accent kicks: double thickness + centre line
    juce::Image barKick2xAccentImage;
    juce::Image barWhiteAccentImage;
    juce::Image barStompImage;           // Elite hi-hat pedal bar, solid yellow (col 10)
    juce::Image barSplashImage;          // Elite hi-hat pedal bar, white body + gold keyline (col 11)

    // Cymbal notes
    juce::Image cymBlueImage;
    juce::Image cymGreenImage;
    juce::Image cymRedImage;
    juce::Image cymWhiteImage;
    juce::Image cymYellowImage;
    juce::Image cymPurpleImage;          // Generated at runtime by recolouring the blue cymbal
    juce::Image cymHiHatClosedImage;     // Extracted gold hi-hat art (elite Hi-Hat lane), used as-is
    juce::Image cymHiHatOpenImage;       // Extracted open hi-hat; hooks in once open/closed parsing lands

    // HOPO notes (colours generated by tinting a greyscale master; white is the SP PNG)
    juce::Image hopoBlueImage;
    juce::Image hopoGreenImage;
    juce::Image hopoOrangeImage;
    juce::Image hopoRedImage;
    juce::Image hopoWhiteImage;
    juce::Image hopoYellowImage;
    juce::Image hopoPurpleImage;

    // Lane graphics
    juce::Image laneEndImage;
    juce::Image laneMidImage;
    juce::Image laneStartImage;

    // Marker graphics
    juce::Image markerBeatImage;
    juce::Image markerHalfBeatImage;
    juce::Image markerMeasureImage;
    juce::Image markerMeasureWriteImage;
    juce::Image markerBeatWriteImage;

    // Returns a copy of `src` with each pixel's alpha amplified to fully
    // saturate (any non-zero alpha → 255). Preserves RGB. Used to build the
    // write-mode marker variants without touching the source PNGs.
    static juce::Image makeAlphaBoostedCopy(const juce::Image& src);

    // Regular notes (colours generated by tinting a greyscale master; white is the SP PNG)
    juce::Image noteBlueImage;
    juce::Image noteGreenImage;
    juce::Image noteOrangeImage;
    juce::Image noteRedImage;
    juce::Image noteWhiteImage;
    juce::Image noteYellowImage;
    juce::Image notePurpleImage;

    // Ghost cursor blanks
    juce::Image noteBlankImage;
    juce::Image hopoBlankImage;
    juce::Image cymBlankImage;
    juce::Image barBlankImage;

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
    juce::Image hitFlareWhite;           // greyscale "smoke" master (was hit_flare_white)
    // Lane-colour -> tinted smoke flare, generated on first use and cleared on
    // rescale. Keyed by the lane colour's ARGB so any colour (preset or custom)
    // is handled with no per-lane asset.
    std::map<juce::uint32, juce::Image> flareTintCache;
    juce::Image* flareTinted(juce::Colour colour);
    juce::Image kickAnimationFrames[7];  // hit_kick_1.png through hit_kick_7.png
    juce::Image openAnimationFrames[7];  // hit_open_1.png through hit_open_7.png
};
