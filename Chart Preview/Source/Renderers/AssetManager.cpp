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

    noteBlueImage = juce::ImageCache::getFromMemory(BinaryData::note_blue_png, BinaryData::note_blue_pngSize);
    noteGreenImage = juce::ImageCache::getFromMemory(BinaryData::note_green_png, BinaryData::note_green_pngSize);
    noteOrangeImage = juce::ImageCache::getFromMemory(BinaryData::note_orange_png, BinaryData::note_orange_pngSize);
    noteRedImage = juce::ImageCache::getFromMemory(BinaryData::note_red_png, BinaryData::note_red_pngSize);
    noteWhiteImage = juce::ImageCache::getFromMemory(BinaryData::note_white_png, BinaryData::note_white_pngSize);
    noteYellowImage = juce::ImageCache::getFromMemory(BinaryData::note_yellow_png, BinaryData::note_yellow_pngSize);

    overlayCymAccentImage = juce::ImageCache::getFromMemory(BinaryData::overlay_cym_accent_png, BinaryData::overlay_cym_accent_pngSize);
    overlayCymGhost80scaleImage = juce::ImageCache::getFromMemory(BinaryData::overlay_cym_ghost_80scale_png, BinaryData::overlay_cym_ghost_80scale_pngSize);
    overlayCymGhostImage = juce::ImageCache::getFromMemory(BinaryData::overlay_cym_ghost_png, BinaryData::overlay_cym_ghost_pngSize);
    overlayNoteAccentImage = juce::ImageCache::getFromMemory(BinaryData::overlay_note_accent_png, BinaryData::overlay_note_accent_pngSize);
    overlayNoteGhostImage = juce::ImageCache::getFromMemory(BinaryData::overlay_note_ghost_png, BinaryData::overlay_note_ghost_pngSize);
    overlayNoteTapImage = juce::ImageCache::getFromMemory(BinaryData::overlay_note_tap_png, BinaryData::overlay_note_tap_pngSize);

    sustainBlueImage = juce::ImageCache::getFromMemory(BinaryData::sustain_blue_png, BinaryData::sustain_blue_pngSize);
    sustainGreenImage = juce::ImageCache::getFromMemory(BinaryData::sustain_green_png, BinaryData::sustain_green_pngSize);
    sustainOpenWhiteImage = juce::ImageCache::getFromMemory(BinaryData::sustain_open_white_png, BinaryData::sustain_open_white_pngSize);
    sustainOpenImage = juce::ImageCache::getFromMemory(BinaryData::sustain_open_png, BinaryData::sustain_open_pngSize);
    sustainOrangeImage = juce::ImageCache::getFromMemory(BinaryData::sustain_orange_png, BinaryData::sustain_orange_pngSize);
    sustainRedImage = juce::ImageCache::getFromMemory(BinaryData::sustain_red_png, BinaryData::sustain_red_pngSize);
    sustainWhiteImage = juce::ImageCache::getFromMemory(BinaryData::sustain_white_png, BinaryData::sustain_white_pngSize);
    sustainYellowImage = juce::ImageCache::getFromMemory(BinaryData::sustain_yellow_png, BinaryData::sustain_yellow_pngSize);
}

juce::Image* AssetManager::getGuitarGlyphImage(Gem gem, uint gemColumn, bool starPowerActive, bool spNoteHeld)
{
    if (starPowerActive && spNoteHeld)
    {
        switch (gem)
        {
        case Gem::HOPO_GHOST:
            switch (gemColumn)
            {
            case 0: return getBarWhiteImage();
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
            case 0: return getBarWhiteImage();
            case 1:
            case 2:
            case 3:
            case 4:
            case 5: return getHopoWhiteImage();
            } break;
        }
    }
    else
    {
        switch (gem)
        {
        case Gem::HOPO_GHOST:
            switch (gemColumn)
            {
            case 0: return getBarOpenImage();
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
        }
    }

    return nullptr;
}

juce::Image* AssetManager::getDrumGlyphImage(Gem gem, uint gemColumn, bool starPowerActive, bool spNoteHeld)
{
    if (starPowerActive && spNoteHeld)
    {
        switch (gem)
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
        }
    } 
    else
    {
        switch (gem)
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
        }
    }

    return nullptr;
}

juce::Image* AssetManager::getSustainImage(uint gemColumn, bool starPowerActive, bool spNoteHeld)
{
    if (starPowerActive && spNoteHeld)
    {
        if (gemColumn == 0)
        {
            return getSustainOpenWhiteImage();
        }
        else
        {
            return getSustainWhiteImage();
        }
    }
    else
    {
        switch (gemColumn)
        {
        case 0: return getSustainOpenImage();
        case 1: return getSustainGreenImage();
        case 2: return getSustainRedImage();
        case 3: return getSustainYellowImage();
        case 4: return getSustainBlueImage();
        case 5: return getSustainOrangeImage();
        }
    }

    return nullptr;
}
