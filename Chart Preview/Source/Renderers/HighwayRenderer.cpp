/*
  ==============================================================================

    HighwayRenderer.cpp
    Created: 15 Jun 2024 3:57:32pm
    Author:  Noah Baxter

  ==============================================================================
*/

#include "HighwayRenderer.h"

HighwayRenderer::HighwayRenderer(juce::ValueTree &state, MidiInterpreter &midiInterpreter)
	: state(state),
	  midiInterpreter(midiInterpreter)
{
	initAssets();
}

HighwayRenderer::~HighwayRenderer()
{
}

void HighwayRenderer::initAssets()
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

void HighwayRenderer::paint(juce::Graphics &g, uint trackWindowStart, uint trackWindowEnd, uint displaySizeInSamples)
{
	TrackWindow trackWindow = midiInterpreter.generateTrackWindow(trackWindowStart, trackWindowEnd);

	// // FAKE DATA
	// TrackWindow trackWindow;
    // trackWindow[trackWindowStart + (int)(1*displaySizeInSamples / 7)] = {Gem::NOTE, Gem::HOPO_GHOST, Gem::HOPO_GHOST, Gem::HOPO_GHOST, Gem::HOPO_GHOST, Gem::NONE, Gem::NONE};
	// trackWindow[trackWindowStart + (int)(2*displaySizeInSamples / 7)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE, Gem::NOTE};
	// trackWindow[trackWindowStart + (int)(2*displaySizeInSamples / 7)] = {Gem::NONE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE, Gem::NONE};
	// trackWindow[trackWindowStart + (int)(3*displaySizeInSamples / 7)] = {Gem::NONE, Gem::TAP_ACCENT, Gem::TAP_ACCENT, Gem::TAP_ACCENT, Gem::TAP_ACCENT, Gem::NONE, Gem::NONE};
	// trackWindow[trackWindowStart + (int)(4*displaySizeInSamples / 7)] = {Gem::NONE, Gem::NONE, Gem::CYM_GHOST, Gem::CYM_GHOST, Gem::CYM_GHOST, Gem::NONE, Gem::NONE};
	// trackWindow[trackWindowStart + (int)(5*displaySizeInSamples / 7)] = {Gem::NONE, Gem::NONE, Gem::CYM, Gem::CYM, Gem::CYM, Gem::NONE, Gem::NONE};
	// trackWindow[trackWindowStart + (int)(6*displaySizeInSamples / 7)] = {Gem::NONE, Gem::NONE, Gem::CYM_ACCENT, Gem::CYM_ACCENT, Gem::CYM_ACCENT, Gem::NONE, Gem::NONE};
    // // Overlapping kicks
	// trackWindow[trackWindowStart + (int)(3*displaySizeInSamples / 7) + 10] = {Gem::NOTE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE};
	// trackWindow[trackWindowStart + (int)(3*displaySizeInSamples / 7) + 5000] = {Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NONE, Gem::NOTE};

    drawCallMap.clear();

    // Populate drawCallMap
    for (auto &frameItem : trackWindow)
	{
		framePosition = frameItem.first;
		float normalizedPosition = (framePosition - trackWindowStart) / (float)displaySizeInSamples;
		drawFrame(frameItem.second, normalizedPosition);
	}

    // Draw layer by layer
    for (const auto& drawOrder : drawCallMap)
    {
        // Draw each layer from back to front
        for (auto it = drawOrder.second.rbegin(); it != drawOrder.second.rend(); ++it)
        {
            (*it)(g);
        }
    }
}


void HighwayRenderer::drawFrame(const std::array<Gem,7> &gems, float position)
{
    uint drawSequence[] = {0, 6, 1, 2, 3, 4, 5};
    for (int i = 0; i < gems.size(); i++)
    {
        int gemColumn = drawSequence[i];
        if (gems[gemColumn] != Gem::NONE)
        {
            drawGem(gemColumn, gems[gemColumn], position);
        }
    }
}

void HighwayRenderer::drawGem(uint gemColumn, Gem gem, float position)
{
    juce::Rectangle<float> glyphRect;
    juce::Image* glyphImage;
    bool barNote;

    if (isPart(state, Part::GUITAR))
    {
        glyphRect = getGuitarGlyphRect(gemColumn, position);
        glyphImage = getGuitarGlyphImage(gem, gemColumn);
        barNote = isBarNote(gemColumn, Part::GUITAR);
    }
    else // if (isPart(state, Part::DRUMS))
    {
        glyphRect = getDrumGlyphRect(gemColumn, position);
        glyphImage = getDrumGlyphImage(gem, gemColumn);
        barNote = isBarNote(gemColumn, Part::DRUMS);
    }

    // No glyph to draw
    if (glyphImage == nullptr)
    {
        return;
    }

    float opacity = calculateOpacity(position);
    if (barNote)
    {
        drawCallMap[DrawOrder::BAR].push_back([=](juce::Graphics &g) {
            draw(g, glyphImage, glyphRect, opacity);
        });
    }
    else
    {
        drawCallMap[DrawOrder::NOTE].push_back([=](juce::Graphics &g) {
            draw(g, glyphImage, glyphRect, opacity);
        });
    }
    
    juce::Image* overlayImage = getOverlayImage(gem);
    if (overlayImage != nullptr)
    {
        juce::Rectangle<float> overlayRect = getOverlayGlyphRect(gem, glyphRect);

        drawCallMap[DrawOrder::OVERLAY].push_back([=](juce::Graphics &g) {
            draw(g, overlayImage, overlayRect, opacity);
        });
    }
}

//==============================================================================
// Glyph positioning

juce::Rectangle<float> HighwayRenderer::createGlyphRect(float position, float normY1, float normY2, float normX1, float normX2, float normWidth1, float normWidth2, bool isBarNote)
{
    // Create rectangle
    float curve = 0.333; // Makes placement a bit more exponential

    int pY1 = normY1 * height;
    int pY2 = normY2 * height;
    int yPos = pY2 - (int)((std::pow(10, curve * (1 - position)) - 1) / (std::pow(10, curve) - 1) * (pY2 - pY1));

    int pX1 = normX1 * width;
    int pX2 = normX2 * width;
    int xPos = pX2 - (int)((std::pow(10, curve * (1 - position)) - 1) / (std::pow(10, curve) - 1) * (pX2 - pX1));

    int pW1 = normWidth1 * width;
    int pW2 = normWidth2 * width;
    int width = pW2 - (int)((std::pow(10, curve * (1 - position)) - 1) / (std::pow(10, curve) - 1) * (pW2 - pW1));

    int height;
    if (isBarNote)
    {
        height = (int)width / 16.f;
    }
    else
    {
        height = (int)width / 2.f;
    }

    return juce::Rectangle<float>(xPos, yPos, width, height);
}

juce::Rectangle<float> HighwayRenderer::getGuitarGlyphRect(uint gemColumn, float position)
{
    float normY1, normY2, normX1, normX2, normWidth1, normWidth2;

    // If the gem is an open note
    bool isOpen = isBarNote(gemColumn, Part::GUITAR);
    if (isOpen)
    {
        normY1 = 0.73;
        normY2 = 0.234;
        normX1 = 0.16;
        normX2 = 0.34;
        normWidth1 = 0.68;
        normWidth2 = 0.32;
    }
    else
    {

        normWidth1 = 0.10, normWidth2 = 0.050;
        normY1 = 0.71, normY2 = 0.22;
        if (gemColumn == 1)
        {
            normX1 = 0.22;
            normX2 = 0.365;
        }
        else if (gemColumn == 2)
        {
            normX1 = 0.332;
            normX2 = 0.420;
        }
        else if (gemColumn == 3)
        {
            normX1 = 0.450;
            normX2 = 0.475;
        }
        else if (gemColumn == 4)
        {
            normX1 = 0.568;
            normX2 = 0.532;
        }
        else if (gemColumn == 5)
        {
            normX1 = 0.680;
            normX2 = 0.590;
        }
    }

    return createGlyphRect(position, normY1, normY2, normX1, normX2, normWidth1, normWidth2, isOpen);
}

juce::Rectangle<float> HighwayRenderer::getDrumGlyphRect(uint gemColumn, float position)
{
    float normY1, normY2, normX1, normX2, normWidth1, normWidth2;

    // If the gem is a kick
    bool isKick = isBarNote(gemColumn, Part::DRUMS);
    if (isKick)
    {
        normY1 = 0.73;
        normY2 = 0.234;
        normX1 = 0.16;
        normX2 = 0.34;
        normWidth1 = 0.68;
        normWidth2 = 0.32;
    }
    else
    {

        normWidth1 = 0.13, normWidth2 = 0.060;
        normY1 = 0.70, normY2 = 0.22;
        if (gemColumn == 1)
        {
            normX1 = 0.22;
            normX2 = 0.365;
        }
        else if (gemColumn == 2)
        {
            normX1 = 0.362;
            normX2 = 0.434;
        }
        else if (gemColumn == 3)
        {
            normX1 = 0.507;
            normX2 = 0.504;
        }
        else if (gemColumn == 4)
        {
            normX1 = 0.650;
            normX2 = 0.570;
        }
    }

    return createGlyphRect(position, normY1, normY2, normX1, normX2, normWidth1, normWidth2, isKick);
}

juce::Rectangle<float> HighwayRenderer::getOverlayGlyphRect(Gem gem, juce::Rectangle<float> glyphRect)
{
    juce::Rectangle<float> overlayRect;
    if (isPart(state, Part::DRUMS) && gem == Gem::TAP_ACCENT)
    {
        float scaleFactor = 1.1232876712; // 12.32876712% larger
        float newWidth = glyphRect.getWidth() * scaleFactor;
        float newHeight = glyphRect.getHeight() * scaleFactor;
        float widthIncrease = newWidth - glyphRect.getWidth();
        float heightIncrease = newHeight - glyphRect.getHeight();

        float xPos = glyphRect.getX() - widthIncrease / 2;
        float yPos = glyphRect.getY() - heightIncrease / 2;

        overlayRect = juce::Rectangle<float>(xPos, yPos, newWidth, newHeight);
    }
    else
    {
        overlayRect = glyphRect;
    }
    
    return overlayRect;
}

//==============================================================================
// Image pickers

juce::Image* HighwayRenderer::getGuitarGlyphImage(Gem gem, uint gemColumn)
{
    using Guitar = MidiPitchDefinitions::Guitar;

    if (state.getProperty("starPower") && midiInterpreter.isNoteHeld((int)Guitar::SP, framePosition))
    {
        switch (gem)
        {
        case Gem::HOPO_GHOST:
            switch (gemColumn)
            {
            case 0: return &barWhiteImage;
            case 1:
            case 2:
            case 3:
            case 4:
            case 5: return &hopoWhiteImage;
            } break;
        case Gem::NOTE:
            switch (gemColumn)
            {
            case 0: return &barWhiteImage;
            case 1:
            case 2:
            case 3:
            case 4:
            case 5: return &noteWhiteImage;
            } break;
        case Gem::TAP_ACCENT:
            switch (gemColumn)
            {
            case 0: return &barWhiteImage;
            case 1:
            case 2:
            case 3:
            case 4:
            case 5: return &hopoWhiteImage;
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
            case 0: return &barOpenImage;
            case 1: return &hopoGreenImage;
            case 2: return &hopoRedImage;
            case 3: return &hopoYellowImage;
            case 4: return &hopoBlueImage;
            case 5: return &hopoOrangeImage;
            } break;
        case Gem::NOTE:
            switch (gemColumn)
            {
            case 0: return &barOpenImage;
            case 1: return &noteGreenImage;
            case 2: return &noteRedImage;
            case 3: return &noteYellowImage;
            case 4: return &noteBlueImage;
            case 5: return &noteOrangeImage;
            } break;
        case Gem::TAP_ACCENT:
            switch (gemColumn)
            {
            case 0: return &barOpenImage;
            case 1: return &hopoGreenImage;
            case 2: return &hopoRedImage;
            case 3: return &hopoYellowImage;
            case 4: return &hopoBlueImage;
            case 5: return &hopoOrangeImage;
            } break;
        }
    }

    return nullptr;
}

juce::Image* HighwayRenderer::getDrumGlyphImage(Gem gem, uint gemColumn)
{
    using Drums = MidiPitchDefinitions::Drums;

    if (state.getProperty("starPower") && midiInterpreter.isNoteHeld((int)Drums::SP, framePosition))
    {
        switch (gem)
        {
        case Gem::HOPO_GHOST:
            switch (gemColumn)
            {
            case 0:
            case 6: return &barWhiteImage;
            case 1:
            case 2:
            case 3:
            case 4: return &hopoWhiteImage;
            } break;
        case Gem::NOTE:
        case Gem::TAP_ACCENT:
            switch (gemColumn)
            {
            case 0:
            case 6: return &barWhiteImage;
            case 1:
            case 2:
            case 3:
            case 4: return &noteWhiteImage;
            } break;
        case Gem::CYM_GHOST:
        case Gem::CYM:
        case Gem::CYM_ACCENT:
            switch (gemColumn)
            {
            case 2:
            case 3:
            case 4: return &cymWhiteImage;
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
            case 1: return &hopoRedImage;
            case 2: return &hopoYellowImage;
            case 3: return &hopoBlueImage;
            case 4: return &hopoGreenImage;
            } break;
        case Gem::NOTE:
        case Gem::TAP_ACCENT:
            switch (gemColumn)
            {
            case 0: return &barKickImage;
            case 6: return &barKick2xImage;
            case 1: return &noteRedImage;
            case 2: return &noteYellowImage;
            case 3: return &noteBlueImage;
            case 4: return &noteGreenImage;
            } break;
        case Gem::CYM_GHOST:
        case Gem::CYM:
        case Gem::CYM_ACCENT:
            switch (gemColumn)
            {
            case 2: return &cymYellowImage;
            case 3: return &cymBlueImage;
            case 4: return &cymGreenImage;
            } break;
        }
    }

    return nullptr;
}

juce::Image* HighwayRenderer::getOverlayImage(Gem gem)
{
    if (isPart(state, Part::GUITAR))
    {
        switch (gem)
        {
        case Gem::TAP_ACCENT: return &overlayNoteTapImage;
        }
    }
    else // if (isPart(state, Part::DRUMS))
    {
        switch (gem)
        {
        case Gem::HOPO_GHOST: return &overlayNoteGhostImage;
        case Gem::TAP_ACCENT: return &overlayNoteAccentImage;
        case Gem::CYM_GHOST: return &overlayCymGhostImage;
        case Gem::CYM_ACCENT: return &overlayCymAccentImage;
        }
    }

    return nullptr;
}