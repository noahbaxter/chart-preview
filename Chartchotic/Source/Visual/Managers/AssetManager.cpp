/*
    ==============================================================================

        AssetManager.cpp
        Created: 15 Jun 2024 3:57:32pm
        Author:  Noah Baxter

    ==============================================================================
*/

#include "AssetManager.h"

AssetManager::AssetManager()
{
    initAssets();
}

AssetManager::~AssetManager()
{
}

void AssetManager::initAssets()
{
#ifndef CHARTCHOTIC_NO_BINARY_DATA
    barKickImage = juce::ImageCache::getFromMemory(BinaryData::bar_kick_png, BinaryData::bar_kick_pngSize);
    barKick2xImage = juce::ImageCache::getFromMemory(BinaryData::bar_kick_2x_png, BinaryData::bar_kick_2x_pngSize);
    barOpenImage = juce::ImageCache::getFromMemory(BinaryData::bar_open_png, BinaryData::bar_open_pngSize);
    barWhiteImage = juce::ImageCache::getFromMemory(BinaryData::bar_white_png, BinaryData::bar_white_pngSize);

    cymBlueImage = juce::ImageCache::getFromMemory(BinaryData::cym_blue_png, BinaryData::cym_blue_pngSize);
    cymGreenImage = juce::ImageCache::getFromMemory(BinaryData::cym_green_png, BinaryData::cym_green_pngSize);
    cymRedImage = juce::ImageCache::getFromMemory(BinaryData::cym_red_png, BinaryData::cym_red_pngSize);
    cymWhiteImage = juce::ImageCache::getFromMemory(BinaryData::cym_white_png, BinaryData::cym_white_pngSize);
    cymYellowImage = juce::ImageCache::getFromMemory(BinaryData::cym_yellow_png, BinaryData::cym_yellow_pngSize);

    hopoBlueImage = juce::ImageCache::getFromMemory(BinaryData::hopo_blue_png, BinaryData::hopo_blue_pngSize);
    hopoGreenImage = juce::ImageCache::getFromMemory(BinaryData::hopo_green_png, BinaryData::hopo_green_pngSize);
    hopoOrangeImage = juce::ImageCache::getFromMemory(BinaryData::hopo_orange_png, BinaryData::hopo_orange_pngSize);
    hopoRedImage = juce::ImageCache::getFromMemory(BinaryData::hopo_red_png, BinaryData::hopo_red_pngSize);
    hopoWhiteImage = juce::ImageCache::getFromMemory(BinaryData::hopo_white_png, BinaryData::hopo_white_pngSize);
    hopoYellowImage = juce::ImageCache::getFromMemory(BinaryData::hopo_yellow_png, BinaryData::hopo_yellow_pngSize);

    laneEndImage = juce::ImageCache::getFromMemory(BinaryData::lane_end_png, BinaryData::lane_end_pngSize);
    laneMidImage = juce::ImageCache::getFromMemory(BinaryData::lane_mid_png, BinaryData::lane_mid_pngSize);
    laneStartImage = juce::ImageCache::getFromMemory(BinaryData::lane_start_png, BinaryData::lane_start_pngSize);

    markerBeatImage = juce::ImageCache::getFromMemory(BinaryData::marker_beat_png, BinaryData::marker_beat_pngSize);
    markerHalfBeatImage = juce::ImageCache::getFromMemory(BinaryData::marker_half_beat_png, BinaryData::marker_half_beat_pngSize);
    markerMeasureImage = juce::ImageCache::getFromMemory(BinaryData::marker_measure_png, BinaryData::marker_measure_pngSize);

    // Write-mode markers: copies of the same source PNGs with their alpha
    // amplified. The original PNGs bake alpha (~0.75 MEASURE / ~0.50 BEAT)
    // which caps gridline opacity even with the renderer's per-type
    // multiplier at 1.0. Boost at load time so write mode can render
    // structural anchors at full opacity through the same perspective sprite
    // path as everything else (no parallel render code).
    markerMeasureWriteImage = makeAlphaBoostedCopy(markerMeasureImage);
    markerBeatWriteImage    = makeAlphaBoostedCopy(markerBeatImage);

    // STEP gridlines reuse the half-beat marker image — same loading path,
    // same scaling, same visual treatment. They're differentiated from
    // half-beats by opacity in GridlineRenderer (STEP 0.25 vs HALF_BEAT 0.35),
    // not by a separate asset. Adding a dedicated thinner PNG is an asset-pack
    // task tracked in BACKLOG.md — do not invent parallel rendering paths.

    noteBlueImage = juce::ImageCache::getFromMemory(BinaryData::note_blue_png, BinaryData::note_blue_pngSize);
    noteGreenImage = juce::ImageCache::getFromMemory(BinaryData::note_green_png, BinaryData::note_green_pngSize);
    noteOrangeImage = juce::ImageCache::getFromMemory(BinaryData::note_orange_png, BinaryData::note_orange_pngSize);
    noteRedImage = juce::ImageCache::getFromMemory(BinaryData::note_red_png, BinaryData::note_red_pngSize);
    noteWhiteImage = juce::ImageCache::getFromMemory(BinaryData::note_white_png, BinaryData::note_white_pngSize);
    noteYellowImage = juce::ImageCache::getFromMemory(BinaryData::note_yellow_png, BinaryData::note_yellow_pngSize);

    overlayCymAccentImage = juce::ImageCache::getFromMemory(BinaryData::overlay_cym_accent_png, BinaryData::overlay_cym_accent_pngSize);
    overlayCymGhostImage = juce::ImageCache::getFromMemory(BinaryData::overlay_cym_ghost_png, BinaryData::overlay_cym_ghost_pngSize);
    overlayNoteAccentImage = juce::ImageCache::getFromMemory(BinaryData::overlay_note_accent_png, BinaryData::overlay_note_accent_pngSize);
    overlayNoteGhostImage = juce::ImageCache::getFromMemory(BinaryData::overlay_note_ghost_png, BinaryData::overlay_note_ghost_pngSize);
    overlayNoteTapImage = juce::ImageCache::getFromMemory(BinaryData::overlay_note_tap_png, BinaryData::overlay_note_tap_pngSize);

    sustainOpenWhiteImage = juce::ImageCache::getFromMemory(BinaryData::sustain_open_white_png, BinaryData::sustain_open_white_pngSize);
    sustainOpenImage = juce::ImageCache::getFromMemory(BinaryData::sustain_open_png, BinaryData::sustain_open_pngSize);

    discoBallImage = juce::ImageCache::getFromMemory(BinaryData::discoball_jpg, BinaryData::discoball_jpgSize);

    // Hit animation frames
    hitAnimationFrames[0] = juce::ImageCache::getFromMemory(BinaryData::hit_flash_1_png, BinaryData::hit_flash_1_pngSize);
    hitAnimationFrames[1] = juce::ImageCache::getFromMemory(BinaryData::hit_flash_2_png, BinaryData::hit_flash_2_pngSize);
    hitAnimationFrames[2] = juce::ImageCache::getFromMemory(BinaryData::hit_flash_3_png, BinaryData::hit_flash_3_pngSize);
    hitAnimationFrames[3] = juce::ImageCache::getFromMemory(BinaryData::hit_flash_4_png, BinaryData::hit_flash_4_pngSize);
    hitAnimationFrames[4] = juce::ImageCache::getFromMemory(BinaryData::hit_flash_5_png, BinaryData::hit_flash_5_pngSize);

    // Hit flare images (blue=4, green=1, orange=5, red=2, yellow=3)
    hitFlareImages[0] = juce::ImageCache::getFromMemory(BinaryData::hit_flare_green_png, BinaryData::hit_flare_green_pngSize);
    hitFlareImages[1] = juce::ImageCache::getFromMemory(BinaryData::hit_flare_red_png, BinaryData::hit_flare_red_pngSize);
    hitFlareImages[2] = juce::ImageCache::getFromMemory(BinaryData::hit_flare_yellow_png, BinaryData::hit_flare_yellow_pngSize);
    hitFlareImages[3] = juce::ImageCache::getFromMemory(BinaryData::hit_flare_blue_png, BinaryData::hit_flare_blue_pngSize);
    hitFlareImages[4] = juce::ImageCache::getFromMemory(BinaryData::hit_flare_orange_png, BinaryData::hit_flare_orange_pngSize);
    hitFlareImages[5] = juce::ImageCache::getFromMemory(BinaryData::hit_flare_white_png, BinaryData::hit_flare_white_pngSize);

    // Generate purple flare by tinting the white (grayscale) flare
    // Purple color matched to tap overlay asset
    {
        auto& src = hitFlareImages[5];
        hitFlarePurpleImage = src.createCopy();
        juce::Image::BitmapData bmp(hitFlarePurpleImage, juce::Image::BitmapData::readWrite);
        const float tintR = 0.55f, tintG = 0.05f, tintB = 1.0f;
        for (int y = 0; y < bmp.height; ++y)
        {
            for (int x = 0; x < bmp.width; ++x)
            {
                auto px = bmp.getPixelColour(x, y);
                float a = px.getFloatAlpha();
                float lum = px.getFloatRed(); // grayscale source: R==G==B
                bmp.setPixelColour(x, y, juce::Colour::fromFloatRGBA(
                    lum * tintR, lum * tintG, lum * tintB, a));
            }
        }
    }

    // Kick animation frames
    kickAnimationFrames[0] = juce::ImageCache::getFromMemory(BinaryData::hit_kick_1_png, BinaryData::hit_kick_1_pngSize);
    kickAnimationFrames[1] = juce::ImageCache::getFromMemory(BinaryData::hit_kick_2_png, BinaryData::hit_kick_2_pngSize);
    kickAnimationFrames[2] = juce::ImageCache::getFromMemory(BinaryData::hit_kick_3_png, BinaryData::hit_kick_3_pngSize);
    kickAnimationFrames[3] = juce::ImageCache::getFromMemory(BinaryData::hit_kick_4_png, BinaryData::hit_kick_4_pngSize);
    kickAnimationFrames[4] = juce::ImageCache::getFromMemory(BinaryData::hit_kick_5_png, BinaryData::hit_kick_5_pngSize);
    kickAnimationFrames[5] = juce::ImageCache::getFromMemory(BinaryData::hit_kick_6_png, BinaryData::hit_kick_6_pngSize);
    kickAnimationFrames[6] = juce::ImageCache::getFromMemory(BinaryData::hit_kick_7_png, BinaryData::hit_kick_7_pngSize);

    // Open animation frames (guitar open notes)
    openAnimationFrames[0] = juce::ImageCache::getFromMemory(BinaryData::hit_open_1_png, BinaryData::hit_open_1_pngSize);
    openAnimationFrames[1] = juce::ImageCache::getFromMemory(BinaryData::hit_open_2_png, BinaryData::hit_open_2_pngSize);
    openAnimationFrames[2] = juce::ImageCache::getFromMemory(BinaryData::hit_open_3_png, BinaryData::hit_open_3_pngSize);
    openAnimationFrames[3] = juce::ImageCache::getFromMemory(BinaryData::hit_open_4_png, BinaryData::hit_open_4_pngSize);
    openAnimationFrames[4] = juce::ImageCache::getFromMemory(BinaryData::hit_open_5_png, BinaryData::hit_open_5_pngSize);
    openAnimationFrames[5] = juce::ImageCache::getFromMemory(BinaryData::hit_open_6_png, BinaryData::hit_open_6_pngSize);
    openAnimationFrames[6] = juce::ImageCache::getFromMemory(BinaryData::hit_open_7_png, BinaryData::hit_open_7_pngSize);

    // Register all per-frame drawable assets for viewport-aware pre-scaling.
    // Full-res originals are copied now; rescaleForWidth() overwrites the active members.
    scalableAssets = {
        // Notes
        {&noteBlueImage, noteBlueImage}, {&noteGreenImage, noteGreenImage},
        {&noteOrangeImage, noteOrangeImage}, {&noteRedImage, noteRedImage},
        {&noteWhiteImage, noteWhiteImage}, {&noteYellowImage, noteYellowImage},
        // Cymbals
        {&cymBlueImage, cymBlueImage}, {&cymGreenImage, cymGreenImage},
        {&cymRedImage, cymRedImage}, {&cymWhiteImage, cymWhiteImage},
        {&cymYellowImage, cymYellowImage},
        // HOPOs
        {&hopoBlueImage, hopoBlueImage}, {&hopoGreenImage, hopoGreenImage},
        {&hopoOrangeImage, hopoOrangeImage}, {&hopoRedImage, hopoRedImage},
        {&hopoWhiteImage, hopoWhiteImage}, {&hopoYellowImage, hopoYellowImage},
        // Bars
        {&barKickImage, barKickImage}, {&barKick2xImage, barKick2xImage},
        {&barOpenImage, barOpenImage}, {&barWhiteImage, barWhiteImage},
        // Overlays
        {&overlayCymAccentImage, overlayCymAccentImage}, {&overlayCymGhostImage, overlayCymGhostImage},
        {&overlayNoteAccentImage, overlayNoteAccentImage}, {&overlayNoteGhostImage, overlayNoteGhostImage},
        {&overlayNoteTapImage, overlayNoteTapImage},
        // Hit flash frames
        {&hitAnimationFrames[0], hitAnimationFrames[0]}, {&hitAnimationFrames[1], hitAnimationFrames[1]},
        {&hitAnimationFrames[2], hitAnimationFrames[2]}, {&hitAnimationFrames[3], hitAnimationFrames[3]},
        {&hitAnimationFrames[4], hitAnimationFrames[4]},
        // Hit flare images
        {&hitFlareImages[0], hitFlareImages[0]}, {&hitFlareImages[1], hitFlareImages[1]},
        {&hitFlareImages[2], hitFlareImages[2]}, {&hitFlareImages[3], hitFlareImages[3]},
        {&hitFlareImages[4], hitFlareImages[4]}, {&hitFlareImages[5], hitFlareImages[5]},
        {&hitFlarePurpleImage, hitFlarePurpleImage},
        // Kick animation frames
        {&kickAnimationFrames[0], kickAnimationFrames[0]}, {&kickAnimationFrames[1], kickAnimationFrames[1]},
        {&kickAnimationFrames[2], kickAnimationFrames[2]}, {&kickAnimationFrames[3], kickAnimationFrames[3]},
        {&kickAnimationFrames[4], kickAnimationFrames[4]}, {&kickAnimationFrames[5], kickAnimationFrames[5]},
        {&kickAnimationFrames[6], kickAnimationFrames[6]},
        // Open animation frames
        {&openAnimationFrames[0], openAnimationFrames[0]}, {&openAnimationFrames[1], openAnimationFrames[1]},
        {&openAnimationFrames[2], openAnimationFrames[2]}, {&openAnimationFrames[3], openAnimationFrames[3]},
        {&openAnimationFrames[4], openAnimationFrames[4]}, {&openAnimationFrames[5], openAnimationFrames[5]},
        {&openAnimationFrames[6], openAnimationFrames[6]},
        // Gridline markers
        {&markerBeatImage, markerBeatImage}, {&markerHalfBeatImage, markerHalfBeatImage},
        {&markerMeasureImage, markerMeasureImage},
        {&markerMeasureWriteImage, markerMeasureWriteImage},
        {&markerBeatWriteImage,    markerBeatWriteImage},
    };
#endif // CHARTCHOTIC_NO_BINARY_DATA
}

juce::Image AssetManager::makeAlphaBoostedCopy(const juce::Image& src)
{
    if (!src.isValid())
        return {};

    juce::Image out = src.createCopy();
    juce::Image::BitmapData bd(out, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < bd.height; ++y)
    {
        juce::uint8* line = bd.getLinePointer(y);
        for (int x = 0; x < bd.width; ++x)
        {
            juce::uint8* px = line + x * bd.pixelStride;
            // ARGB byte order on macOS/Win is BGRA in memory; alpha is the
            // 4th byte regardless. juce::PixelARGB stores it as the A
            // component — use the explicit getter for portability.
            juce::PixelARGB* pixel = reinterpret_cast<juce::PixelARGB*>(px);
            if (pixel->getAlpha() > 0)
                pixel->setAlpha(255);
        }
    }
    return out;
}

juce::Image AssetManager::downscale(const juce::Image& src, int targetWidth)
{
    if (!src.isValid() || targetWidth <= 0 || targetWidth >= src.getWidth())
        return src;

    float ratio = (float)targetWidth / (float)src.getWidth();
    int targetHeight = std::max(1, (int)(src.getHeight() * ratio));

    juce::Image scaled(juce::Image::ARGB, targetWidth, targetHeight, true);
    juce::Graphics g(scaled);
    g.drawImage(src, juce::Rectangle<float>(0, 0, (float)targetWidth, (float)targetHeight));
    return scaled;
}

void AssetManager::rescaleForWidth(int viewportWidth)
{
    // Target: half the viewport width — generous headroom for strikeline-sized notes
    int targetWidth = std::max(200, viewportWidth / 2);

    if (targetWidth == lastScaledWidth)
        return;

    for (auto& asset : scalableAssets)
        *asset.target = downscale(asset.fullRes, targetWidth);

    lastScaledWidth = targetWidth;
}

juce::Image* AssetManager::getGuitarGlyphImage(const GemWrapper& gemWrapper, uint gemColumn, bool starPowerActive)
{
    // Use the gem's star power flag to determine if it should be white
    bool shouldBeWhite = starPowerActive && gemWrapper.starPower;

    if (shouldBeWhite)
    {
        switch (gemWrapper.gem)
        {
        case Gem::HOPO_GHOST:
            switch (gemColumn)
            {
            case 0: return getHopoBarWhiteImage();
            case 1:
            case 2:
            case 3:
            case 4:
            case 5: return getHopoWhiteImage();
            } break;
        case Gem::NOTE:
            switch (gemColumn)
            {
            case 0: return getBarWhiteImage();
            case 1:
            case 2:
            case 3:
            case 4:
            case 5: return getNoteWhiteImage();
            } break;
        case Gem::TAP_ACCENT:
            switch (gemColumn)
            {
            case 0: return getHopoBarWhiteImage();
            case 1:
            case 2:
            case 3:
            case 4:
            case 5: return getHopoWhiteImage();
            } break;
        default: break;
        }
    }
    else
    {
        switch (gemWrapper.gem)
        {
        case Gem::HOPO_GHOST:
            switch (gemColumn)
            {
            case 0: return getHopoBarOpenImage();
            case 1: return getHopoGreenImage();
            case 2: return getHopoRedImage();
            case 3: return getHopoYellowImage();
            case 4: return getHopoBlueImage();
            case 5: return getHopoOrangeImage();
            } break;
        case Gem::NOTE:
            switch (gemColumn)
            {
            case 0: return getBarOpenImage();
            case 1: return getNoteGreenImage();
            case 2: return getNoteRedImage();
            case 3: return getNoteYellowImage();
            case 4: return getNoteBlueImage();
            case 5: return getNoteOrangeImage();
            } break;
        case Gem::TAP_ACCENT:
            switch (gemColumn)
            {
            case 0: return getBarOpenImage();
            case 1: return getHopoGreenImage();
            case 2: return getHopoRedImage();
            case 3: return getHopoYellowImage();
            case 4: return getHopoBlueImage();
            case 5: return getHopoOrangeImage();
            } break;
        default: break;
        }
    }

    return nullptr;
}

juce::Image* AssetManager::getDrumGlyphImage(const GemWrapper& gemWrapper, uint gemColumn, bool starPowerActive)
{
    // Use the gem's star power flag to determine if it should be white
    bool shouldBeWhite = starPowerActive && gemWrapper.starPower;

    if (shouldBeWhite)
    {
        switch (gemWrapper.gem)
        {
        case Gem::HOPO_GHOST:
            switch (gemColumn)
            {
            case 0:
            case 6: return getBarWhiteImage();
            case 1:
            case 2:
            case 3:
            case 4: return getHopoWhiteImage();
            } break;
        case Gem::NOTE:
        case Gem::TAP_ACCENT:
            switch (gemColumn)
            {
            case 0:
            case 6: return getBarWhiteImage();
            case 1:
            case 2:
            case 3:
            case 4: return getNoteWhiteImage();
            } break;
        case Gem::CYM_GHOST:
        case Gem::CYM:
        case Gem::CYM_ACCENT:
            switch (gemColumn)
            {
            case 2:
            case 3:
            case 4: return getCymWhiteImage();
            } break;
        default: break;
        }
    }
    else
    {
        switch (gemWrapper.gem)
        {
        case Gem::HOPO_GHOST:
            switch (gemColumn)
            {
            case 1: return getHopoRedImage();
            case 2: return getHopoYellowImage();
            case 3: return getHopoBlueImage();
            case 4: return getHopoGreenImage();
            } break;
        case Gem::NOTE:
        case Gem::TAP_ACCENT:
            switch (gemColumn)
            {
            case 0: return getBarKickImage();
            case 6: return getBarKick2xImage();
            case 1: return getNoteRedImage();
            case 2: return getNoteYellowImage();
            case 3: return getNoteBlueImage();
            case 4: return getNoteGreenImage();
            } break;
        case Gem::CYM_GHOST:
        case Gem::CYM:
        case Gem::CYM_ACCENT:
            switch (gemColumn)
            {
            case 2: return getCymYellowImage();
            case 3: return getCymBlueImage();
            case 4: return getCymGreenImage();
            } break;
        default: break;
        }
    }

    return nullptr;
}

juce::Image* AssetManager::getGridlineImage(Gridline gridlineType)
{
    switch (gridlineType)
    {
    case Gridline::MEASURE: return getMarkerMeasureImage();
    case Gridline::BEAT: return getMarkerBeatImage();
    case Gridline::HALF_BEAT: return getMarkerHalfBeatImage();
    case Gridline::STEP: return getMarkerHalfBeatImage(); // alias — opacity differentiates
    }

    return nullptr;
}

juce::Image* AssetManager::getOverlayImage(Gem gem, Part part)
{
    if (part == Part::GUITAR)
    {
        switch (gem)
        {
        case Gem::TAP_ACCENT: return getOverlayNoteTapImage();
        default: break;
        }
    }
    else // if (part == Part::DRUMS)
    {
        switch (gem)
        {
        case Gem::HOPO_GHOST: return getOverlayNoteGhostImage();
        case Gem::TAP_ACCENT: return getOverlayNoteAccentImage();
        case Gem::CYM_GHOST: return getOverlayCymGhostImage();
        case Gem::CYM_ACCENT: return getOverlayCymAccentImage();
        default: break;
        }
    }

    return nullptr;
}

juce::Colour AssetManager::getLaneColour(uint gemColumn, Part part, bool starPowerActive)
{
    if (starPowerActive)
    {
        return juce::Colours::white;
    }

    if (part == Part::GUITAR)
    {
        juce::Colour guitarColors[] = {
            juce::Colours::purple,
            juce::Colours::green,
            juce::Colours::red,
            juce::Colours::yellow,
            juce::Colours::blue,
            juce::Colours::orange
        };
        return guitarColors[std::min(gemColumn, 5u)];
    }
    else // if (part == Part::DRUMS)
    {
        juce::Colour drumColors[] = {
            juce::Colours::orange,
            juce::Colours::red,
            juce::Colours::yellow,
            juce::Colours::blue,
            juce::Colours::green
        };
        uint idx = std::min(drumColumnIndex(gemColumn), 4u);
        return drumColors[idx];
    }
}
