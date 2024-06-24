/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ChartPreviewAudioProcessorEditor::ChartPreviewAudioProcessorEditor(ChartPreviewAudioProcessor &p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      midiInterpreter(state, audioProcessor.getNoteStateMapArray())
{
    setSize(defaultWidth, defaultHeight);

    latencyInSeconds = audioProcessor.latencyInSeconds;
    initState();
    initAssets();
    initMenus();

    startTimerHz(60);
}

ChartPreviewAudioProcessorEditor::~ChartPreviewAudioProcessorEditor()
{
}

void ChartPreviewAudioProcessorEditor::initState()
{
    state = juce::ValueTree("state");
    state.setProperty("skillLevel", (int)SkillLevel::EXPERT, nullptr);
    state.setProperty("part", (int)Part::DRUMS, nullptr);
    state.setProperty("drumType", (int)DrumType::PRO, nullptr);

    state.setProperty("starPower", true, nullptr);
    state.setProperty("kick2x", true, nullptr);
    state.setProperty("dynamics", true, nullptr);

    state.setProperty("framerate", 3, nullptr);
}

void ChartPreviewAudioProcessorEditor::initAssets()
{
    backgroundImage = juce::ImageCache::getFromMemory(BinaryData::background_png, BinaryData::background_pngSize);

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

    trackDrumImage = juce::ImageCache::getFromMemory(BinaryData::track_drum_png, BinaryData::track_drum_pngSize);
    trackGuitarImage = juce::ImageCache::getFromMemory(BinaryData::track_guitar_png, BinaryData::track_guitar_pngSize);
}

void ChartPreviewAudioProcessorEditor::initMenus()
{
    // Create menus
    skillMenu.addItemList(skillLevelLabels, 1);
    skillMenu.setSelectedId(state.getProperty("skillLevel"), juce::NotificationType::dontSendNotification);
    skillMenu.addListener(this);
    addAndMakeVisible(skillMenu);

    partMenu.addItemList(partLabels, 1);
    partMenu.setSelectedId(state.getProperty("part"), juce::NotificationType::dontSendNotification);
    partMenu.addListener(this);
    addAndMakeVisible(partMenu);

    drumTypeMenu.addItemList(drumTypeLabels, 1);
    drumTypeMenu.setSelectedId(state.getProperty("drumType"), juce::NotificationType::dontSendNotification);
    drumTypeMenu.addListener(this);
    addAndMakeVisible(drumTypeMenu);

    framerateMenu.addItemList({"15 fps", "30 fps", "60 fps"}, 1);
    framerateMenu.setSelectedId(state.getProperty("framerate"), juce::NotificationType::dontSendNotification);
    framerateMenu.addListener(this);
    addAndMakeVisible(framerateMenu);
    
    chartZoomSlider.setRange(0.40, 1.2, 0.05);
    chartZoomSlider.setValue(0.60);
    chartZoomSlider.setSliderStyle(juce::Slider::LinearVertical);
    chartZoomSlider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, 50, 20);
    chartZoomSlider.addListener(this);
    addAndMakeVisible(chartZoomSlider);

    chartZoomLabel.setText("Zoom", juce::dontSendNotification);
    addAndMakeVisible(chartZoomLabel);

    // Create toggles
    starPowerToggle.setButtonText("Star Power");
    starPowerToggle.setToggleState(true, juce::NotificationType::dontSendNotification);
    starPowerToggle.addListener(this);
    addAndMakeVisible(starPowerToggle);
    
    kick2xToggle.setButtonText("Kick 2x");
    kick2xToggle.setToggleState(true, juce::NotificationType::dontSendNotification);
    kick2xToggle.addListener(this);
    addAndMakeVisible(kick2xToggle);
    
    dynamicsToggle.setButtonText("Dynamics");
    dynamicsToggle.setToggleState(true, juce::NotificationType::dontSendNotification);
    dynamicsToggle.addListener(this);
    addAndMakeVisible(dynamicsToggle);

    // Debug toggle
    debugToggle.setButtonText("Debug");
    addAndMakeVisible(debugToggle);

    // Create console output
    consoleOutput.setMultiLine(true);
    consoleOutput.setReadOnly(true);
    addAndMakeVisible(consoleOutput);
}

//==============================================================================
void ChartPreviewAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.drawImage(backgroundImage, getLocalBounds().toFloat());

    // Draw the track
    if (isPart(Part::DRUMS))
    {
        g.drawImage(trackDrumImage, juce::Rectangle<float>(0, 0, getWidth(), getHeight()), juce::RectanglePlacement::centred);
    }
    else if (isPart(Part::GUITAR))
    {
        g.drawImage(trackGuitarImage, juce::Rectangle<float>(0, 0, getWidth(), getHeight()), juce::RectanglePlacement::centred);
    }

    drumTypeMenu.setVisible(isPart(Part::DRUMS));
    kick2xToggle.setVisible(isPart(Part::DRUMS));
    dynamicsToggle.setVisible(isPart(Part::DRUMS));
    
    consoleOutput.setVisible(debugToggle.getToggleState());

    //==============================================================================
    // Draw track window
    uint trackWindowStart = currentPlayheadPositionInSamples();
    if (audioProcessor.isPlaying)
    {
        // Shift the start position back by the latency to account for the delay
        trackWindowStart = std::max(0, (int)trackWindowStart - int(latencyInSeconds * audioProcessor.getSampleRate()));
    }
    uint trackWindowEnd = trackWindowStart + displaySizeInSamples;

    TrackWindow trackWindow = midiInterpreter.generateTrackWindow(trackWindowStart, trackWindowEnd);

    // // FAKE DATA
    // TrackWindow trackWindow;
    // trackWindow[trackWindowStart + 1] = {Gem::NOTE, Gem::HOPO_GHOST, Gem::HOPO_GHOST, Gem::HOPO_GHOST, Gem::HOPO_GHOST, Gem::NONE, Gem::NONE};
    // trackWindow[trackWindowStart + (int)(1*displaySizeInSamples / 6)] = {Gem::NONE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE, Gem::NONE};
    // trackWindow[trackWindowStart + (int)(2*displaySizeInSamples / 6)] = {Gem::NONE, Gem::TAP_ACCENT, Gem::TAP_ACCENT, Gem::TAP_ACCENT, Gem::TAP_ACCENT, Gem::NONE, Gem::NONE};
    // trackWindow[trackWindowStart + (int)(3*displaySizeInSamples / 6)] = {Gem::NONE, Gem::NONE, Gem::CYM_GHOST, Gem::CYM_GHOST, Gem::CYM_GHOST, Gem::NONE, Gem::NONE};
    // trackWindow[trackWindowStart + (int)(4*displaySizeInSamples / 6)] = {Gem::NONE, Gem::NONE, Gem::CYM, Gem::CYM, Gem::CYM, Gem::NONE, Gem::NONE};
    // trackWindow[trackWindowStart + (int)(5*displaySizeInSamples / 6)] = {Gem::NONE, Gem::NONE, Gem::CYM_ACCENT, Gem::CYM_ACCENT, Gem::CYM_ACCENT, Gem::NONE, Gem::NONE};

    for (auto &frameItem : trackWindow)
    {
        framePosition = frameItem.first;
        float normalizedPosition = (framePosition - trackWindowStart) / (float)displaySizeInSamples;
        drawFrame(g, frameItem.second, normalizedPosition);
    }
}

void ChartPreviewAudioProcessorEditor::resized()
{
    skillMenu.setBounds(10, 10, 100, 20);
    partMenu.setBounds(120, 10, 100, 20);
    drumTypeMenu.setBounds(230, 10, 100, 20);

    starPowerToggle.setBounds(getWidth() - 120, 10, 100, 20);
    kick2xToggle.setBounds(getWidth() - 120, 30, 100, 20);
    dynamicsToggle.setBounds(getWidth() - 120, 50, 100, 20);

    framerateMenu.setBounds(getWidth() - 120, getHeight() - 30, 100, 20);
    
    chartZoomLabel.setBounds(getWidth() - 90, getHeight() - 230, 40, 20);
    chartZoomSlider.setBounds(getWidth() - 120, getHeight() - 200, 100, 150);

    debugToggle.setBounds(340, 10, 100, 20);

    consoleOutput.setBounds(10, 40, getWidth() - 20, getHeight() - 50);
}

// Draw Functions
//==============================================================================

void ChartPreviewAudioProcessorEditor::drawFrame(juce::Graphics &g, const std::array<Gem,7> &gems, float position)
{
    for (int gemColumn = 0; gemColumn < gems.size(); gemColumn++)
    {
        if (gems[gemColumn] != Gem::NONE)
        {
            if (isPart(Part::DRUMS))
            {
                drawDrumGem(g, gemColumn, gems[gemColumn], position);
            }
            else if (isPart(Part::GUITAR))
            {
                drawGuitarGem(g, gemColumn, gems[gemColumn], position);
            }
        }
    }
}

void ChartPreviewAudioProcessorEditor::drawGuitarGem(juce::Graphics &g, uint gemColumn, Gem gem, float position)
{
    juce::Rectangle<float> glyphRect = getGuitarGlyphRect(gemColumn, position);
    juce::Image glyphImage = getGuitarGlyphImage(gem, gemColumn);
    fadeInImage(glyphImage, position);

    g.drawImage(glyphImage, glyphRect);
}


void ChartPreviewAudioProcessorEditor::drawDrumGem(juce::Graphics &g, uint gemColumn, Gem gem, float position)
{
    juce::Rectangle<float> glyphRect = getDrumGlyphRect(gemColumn, position);
    juce::Image glyphImage = getDrumGlyphImage(gem, gemColumn);
    fadeInImage(glyphImage, position);
    g.drawImage(glyphImage, glyphRect);

    if (gem == Gem::TAP_ACCENT)
    {
        // Adjust accent flair scale/position
        float scaleFactor = 1.1232876712; // 12.32876712% larger
        float newWidth = glyphRect.getWidth() * scaleFactor;
        float newHeight = glyphRect.getHeight() * scaleFactor;
        float widthIncrease = newWidth - glyphRect.getWidth();
        float heightIncrease = newHeight - glyphRect.getHeight();

        juce::Rectangle<float> accentRect = glyphRect.withWidth(newWidth)
                                                     .withHeight(newHeight)
                                                     .withX(glyphRect.getX() - widthIncrease / 2)
                                                     .withY(glyphRect.getY() - heightIncrease / 2);
        g.drawImage(overlayNoteAccentImage, accentRect);
    }
    else if (gem == Gem::CYM_ACCENT)
    {
        // g.drawImage(glyphImage, glyphRect);
    }
}

void ChartPreviewAudioProcessorEditor::fadeInImage(juce::Image &image, float position)
{
    // Make the gem fade out as it gets closer to the end
    float opacity;
    float opacityStart = 0.9;
    if (position >= opacityStart)
    {
        float opacity = 1.0 - ((position - opacityStart) / (1.0 - opacityStart));
        image.multiplyAllAlphas(opacity);
    }
}

juce::Rectangle<float> ChartPreviewAudioProcessorEditor::getGuitarGlyphRect(uint gemColumn, float position)
{
    float normY1, normY2, normX1, normX2, normWidth1, normWidth2;

    // If the gem is an open note
    bool isOpen = (gemColumn == 0);
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

juce::Rectangle<float> ChartPreviewAudioProcessorEditor::getDrumGlyphRect(uint gemColumn, float position)
{
    float normY1, normY2, normX1, normX2, normWidth1, normWidth2;

    // If the gem is a kick
    bool isKick = (gemColumn == 0 || gemColumn == 6);
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

juce::Rectangle<float> ChartPreviewAudioProcessorEditor::createGlyphRect(float position, float normY1, float normY2, float normX1, float normX2, float normWidth1, float normWidth2, bool isBarNote)
{
    // Create rectangle
    float curve = 0.333; // Makes placement a bit more exponential

    int pY1 = normY1 * getHeight();
    int pY2 = normY2 * getHeight();
    int yPos = pY2 - (int)((std::pow(10, curve * (1 - position)) - 1) / (std::pow(10, curve) - 1) * (pY2 - pY1));

    int pX1 = normX1 * getWidth();
    int pX2 = normX2 * getWidth();
    int xPos = pX2 - (int)((std::pow(10, curve * (1 - position)) - 1) / (std::pow(10, curve) - 1) * (pX2 - pX1));

    int pW1 = normWidth1 * getWidth();
    int pW2 = normWidth2 * getWidth();
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

juce::Image ChartPreviewAudioProcessorEditor::getDrumGlyphImage(Gem gem, uint gemColumn)
{
    using Drums = MidiPitchDefinitions::Drums;
    juce::Image gemImage;

    if (state.getProperty("starPower") && midiInterpreter.isNoteHeld((int)Drums::SP, framePosition))
    {
        switch (gem)
        {
        case Gem::HOPO_GHOST:
        case Gem::NOTE:
        case Gem::TAP_ACCENT:
            switch (gemColumn)
            {
            case 0: gemImage = barWhiteImage.createCopy(); break;
            case 1:
            case 2:
            case 3:
            case 4: gemImage = noteWhiteImage.createCopy(); break;
            case 6: gemImage = barWhiteImage.createCopy(); break;
            } break;
        case Gem::CYM_GHOST:
        case Gem::CYM:
        case Gem::CYM_ACCENT:
            switch (gemColumn)
            {
            case 2:
            case 3:
            case 4: gemImage = cymWhiteImage.createCopy(); break;
            } break;
        }
    } 
    else
    {
        switch (gem)
        {
        case Gem::HOPO_GHOST:
        case Gem::NOTE:
        case Gem::TAP_ACCENT:
            switch (gemColumn)
            {
            case 0: gemImage = barKickImage.createCopy(); break;
            case 1: gemImage = noteRedImage.createCopy(); break;
            case 2: gemImage = noteYellowImage.createCopy(); break;
            case 3: gemImage = noteBlueImage.createCopy(); break;
            case 4: gemImage = noteGreenImage.createCopy(); break;
            case 6: gemImage = barKick2xImage.createCopy(); break;
            } break;
        case Gem::CYM_GHOST:
        case Gem::CYM:
        case Gem::CYM_ACCENT:
            switch (gemColumn)
            {
            case 2: gemImage = cymYellowImage.createCopy(); break;
            case 3: gemImage = cymBlueImage.createCopy(); break;
            case 4: gemImage = cymGreenImage.createCopy(); break;
            } break;
        }
    }

    bool isGhost = (gem == Gem::HOPO_GHOST || gem == Gem::CYM_GHOST);
    if (isGhost)
    {
        gemImage.multiplyAllAlphas(0.5f);
    }

    return gemImage;
}

juce::Image ChartPreviewAudioProcessorEditor::getGuitarGlyphImage(Gem gem, uint gemColumn)
{
    using Guitar = MidiPitchDefinitions::Guitar;
    juce::Image gemImage;

    if (state.getProperty("starPower") && midiInterpreter.isNoteHeld((int)Guitar::SP, framePosition))
    {
        switch (gem)
        {
        case Gem::HOPO_GHOST:
            switch (gemColumn)
            {
            case 0: gemImage = barWhiteImage.createCopy(); break;
            case 1:
            case 2:
            case 3:
            case 4:
            case 5: gemImage = hopoWhiteImage.createCopy(); break;
            } break;
        case Gem::NOTE:
        case Gem::TAP_ACCENT:
            switch (gemColumn)
            {
            case 0: gemImage = barWhiteImage.createCopy(); break;
            case 1:
            case 2:
            case 3:
            case 4:
            case 5: gemImage = noteWhiteImage.createCopy(); break;
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
            case 0: gemImage = barOpenImage.createCopy(); break;
            case 1: gemImage = hopoGreenImage.createCopy(); break;
            case 2: gemImage = hopoRedImage.createCopy(); break;
            case 3: gemImage = hopoYellowImage.createCopy(); break;
            case 4: gemImage = hopoBlueImage.createCopy(); break;
            case 5: gemImage = hopoOrangeImage.createCopy(); break;
            } break;
        case Gem::NOTE:
        case Gem::TAP_ACCENT:
            switch (gemColumn)
            {
            case 0: gemImage = barOpenImage.createCopy(); break;
            case 1: gemImage = noteGreenImage.createCopy(); break;
            case 2: gemImage = noteRedImage.createCopy(); break;
            case 3: gemImage = noteYellowImage.createCopy(); break;
            case 4: gemImage = noteBlueImage.createCopy(); break;
            case 5: gemImage = noteOrangeImage.createCopy(); break;
            } break;
        }
    }

    return gemImage;
}
