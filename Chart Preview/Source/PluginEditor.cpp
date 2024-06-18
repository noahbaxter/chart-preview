/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ChartPreviewAudioProcessorEditor::ChartPreviewAudioProcessorEditor (ChartPreviewAudioProcessor& p)
    : AudioProcessorEditor (&p), 
      audioProcessor (p), 
      midiInterpreter(state)
{
    startTimerHz(60);

    setSize(defaultWidth, defaultHeight);

    initState();
    initAssets();
    initMenus();

    midiInterpreter.isNoteHeld = [this](int note) -> bool
    {
        return this->isNoteHeld(note);
    };
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
}

void ChartPreviewAudioProcessorEditor::initAssets()
{
    backgroundImage = juce::ImageCache::getFromMemory(BinaryData::RBN_background1_png, BinaryData::RBN_background1_pngSize);
    drumTrackImage = juce::ImageCache::getFromMemory(BinaryData::track_drum_png, BinaryData::track_drum_pngSize);
    guitarTrackImage = juce::ImageCache::getFromMemory(BinaryData::track_guitar_png, BinaryData::track_guitar_pngSize);

    halfBeatImage = juce::ImageCache::getFromMemory(BinaryData::half_beat_marker_png, BinaryData::half_beat_marker_pngSize);
    measureImage = juce::ImageCache::getFromMemory(BinaryData::measure_png, BinaryData::measure_pngSize);

    gemKickImage = juce::ImageCache::getFromMemory(BinaryData::gem_kick_png, BinaryData::gem_kick_pngSize);
    gemGreenImage = juce::ImageCache::getFromMemory(BinaryData::gem_green_png, BinaryData::gem_green_pngSize);
    gemRedImage = juce::ImageCache::getFromMemory(BinaryData::gem_red_png, BinaryData::gem_red_pngSize);
    gemYellowImage = juce::ImageCache::getFromMemory(BinaryData::gem_yellow_png, BinaryData::gem_yellow_pngSize);
    gemBlueImage = juce::ImageCache::getFromMemory(BinaryData::gem_blue_png, BinaryData::gem_blue_pngSize);
    gemOrangeImage = juce::ImageCache::getFromMemory(BinaryData::gem_orange_png, BinaryData::gem_orange_pngSize);
    gemKickStyleImage = juce::ImageCache::getFromMemory(BinaryData::gem_kick_style_png, BinaryData::gem_kick_style_pngSize);
    gemStyleImage = juce::ImageCache::getFromMemory(BinaryData::gem_style_png, BinaryData::gem_style_pngSize);

    gemHOPOGreenImage = juce::ImageCache::getFromMemory(BinaryData::gem_hopo_green_png, BinaryData::gem_hopo_green_pngSize);
    gemHOPORedImage = juce::ImageCache::getFromMemory(BinaryData::gem_hopo_red_png, BinaryData::gem_hopo_red_pngSize);
    gemHOPOYellowImage = juce::ImageCache::getFromMemory(BinaryData::gem_hopo_yellow_png, BinaryData::gem_hopo_yellow_pngSize);
    gemHOPOBlueImage = juce::ImageCache::getFromMemory(BinaryData::gem_hopo_blue_png, BinaryData::gem_hopo_blue_pngSize);
    gemHOPOOrangeImage = juce::ImageCache::getFromMemory(BinaryData::gem_hopo_orange_png, BinaryData::gem_hopo_orange_pngSize);
    gemHOPOStyleImage = juce::ImageCache::getFromMemory(BinaryData::gem_hopo_style_png, BinaryData::gem_hopo_style_pngSize);

    gemCymYellowImage = juce::ImageCache::getFromMemory(BinaryData::gem_cym_yellow_png, BinaryData::gem_cym_yellow_pngSize);
    gemCymBlueImage = juce::ImageCache::getFromMemory(BinaryData::gem_cym_blue_png, BinaryData::gem_cym_blue_pngSize);
    gemCymGreenImage = juce::ImageCache::getFromMemory(BinaryData::gem_cym_green_png, BinaryData::gem_cym_green_pngSize);
    gemCymStyleImage = juce::ImageCache::getFromMemory(BinaryData::gem_cym_style_png, BinaryData::gem_cym_style_pngSize);
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
        g.drawImage(drumTrackImage, juce::Rectangle<float>(0, 0, getWidth(), getHeight()), juce::RectanglePlacement::centred);
    }
    else if (isPart(Part::GUITAR))
    {
        g.drawImage(guitarTrackImage, juce::Rectangle<float>(0, 0, getWidth(), getHeight()), juce::RectanglePlacement::centred);
    }

    drumTypeMenu.setVisible(isPart(Part::DRUMS));
    kick2xToggle.setVisible(isPart(Part::DRUMS));
    dynamicsToggle.setVisible(isPart(Part::DRUMS));
    
    consoleOutput.setVisible(debugToggle.getToggleState());

    // // Draw lines
    // for (int i = 0; i < beatLines.size(); i++)
    // {
    //     drawMeasureLine(g, beatLines[i].position, beatLines[i].measure);
    // }

    // Draw Buffer
    auto midiEventMap = audioProcessor.getMidiEventMap();

    int lower = std::max(0, currentPlayheadPositionInSamples() - 1);
    int upper = currentPlayheadPositionInSamples() + displaySizeInSamples;
    auto lowerMEM = midiEventMap.lower_bound(lower);
    auto upperMEM = midiEventMap.upper_bound(upper);
    for (auto it = lowerMEM; it != upperMEM; ++it)
    {
        noteHeldPosition = it->first;
        float normalizedPosition = (noteHeldPosition - currentPlayheadPositionInSamples()) / (float)displaySizeInSamples;

        std::array<Gem, 7> gems = midiInterpreter.interpretMidiFrame(it->second);

        // TODO: Find distance from previous note to see if auto HOPO

        // TODO: Find distance to next note off to see if sustain

        drawFrame(g, gems, normalizedPosition);

        // std::string str = std::to_string(round(normalizedPosition * 1000) / 1000) + ": " + 
        //     gemsToString(gems) + " SP; " + std::to_string(noteStates[(int)MidiPitchDefinitions::Drums::SP]);
        // print(str);

        
    }

    // using Drums = MidiPitchDefinitions::Drums;
    // noteHeldPosition = currentPlayheadPositionInSamples();
    // std::string str = "<" +
    //     std::to_string(isNoteHeld((int)Drums::EXPERT_KICK)) + ", " +
    //     std::to_string(isNoteHeld((int)Drums::EXPERT_RED)) + ", " +
    //     std::to_string(isNoteHeld((int)Drums::EXPERT_YELLOW)) + ", " +
    //     std::to_string(isNoteHeld((int)Drums::EXPERT_BLUE)) + ", " +
    //     std::to_string(isNoteHeld((int)Drums::EXPERT_GREEN)) + ", " +
    //     std::to_string(isNoteHeld((int)Drums::EXPERT_KICK_2X)) + "> <" +
    //     std::to_string(isNoteHeld((int)Drums::TOM_YELLOW)) + ", " +
    //     std::to_string(isNoteHeld((int)Drums::TOM_BLUE)) + ", " +
    //     std::to_string(isNoteHeld((int)Drums::TOM_GREEN)) + "> SP: " +
    //     std::to_string(isNoteHeld((int)Drums::SP));
    // print(str);
}


void ChartPreviewAudioProcessorEditor::resized()
{
    skillMenu.setBounds(10, 10, 100, 20);
    partMenu.setBounds(120, 10, 100, 20);
    drumTypeMenu.setBounds(230, 10, 100, 20);

    starPowerToggle.setBounds(getWidth() - 120, 10, 100, 20);
    kick2xToggle.setBounds(getWidth() - 120, 30, 100, 20);
    dynamicsToggle.setBounds(getWidth() - 120, 50, 100, 20);

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

    if (state.getProperty("starPower") && isNoteHeld((int)Drums::SP))
    {
        switch (gem)
        {
        case Gem::HOPO_GHOST:
        case Gem::NOTE:
        case Gem::TAP_ACCENT:
            switch (gemColumn)
            {
            case 0: gemImage = gemKickStyleImage.createCopy(); break;
            case 1:
            case 2:
            case 3:
            case 4: gemImage = gemStyleImage.createCopy(); break;
            case 6: gemImage = gemKickStyleImage.createCopy(); break;
            } break;
        case Gem::CYM_GHOST:
        case Gem::CYM:
        case Gem::CYM_ACCENT:
            switch (gemColumn)
            {
            case 2:
            case 3:
            case 4: gemImage = gemCymStyleImage.createCopy(); break;
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
            case 0: gemImage = gemKickImage.createCopy(); break;
            case 1: gemImage = gemRedImage.createCopy(); break;
            case 2: gemImage = gemYellowImage.createCopy(); break;
            case 3: gemImage = gemBlueImage.createCopy(); break;
            case 4: gemImage = gemGreenImage.createCopy(); break;
            case 6: gemImage = gemKickImage.createCopy(); break;
            } break;
        case Gem::CYM_GHOST:
        case Gem::CYM:
        case Gem::CYM_ACCENT:
            switch (gemColumn)
            {
            case 2: gemImage = gemCymYellowImage.createCopy(); break;
            case 3: gemImage = gemCymBlueImage.createCopy(); break;
            case 4: gemImage = gemCymGreenImage.createCopy(); break;
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

    if (state.getProperty("starPower") && isNoteHeld((int)Guitar::SP))
    {
        switch (gem)
        {
        case Gem::HOPO_GHOST:
            switch (gemColumn)
            {
            case 0: gemImage = gemKickStyleImage.createCopy(); break;
            case 1:
            case 2:
            case 3:
            case 4:
            case 5: gemImage = gemHOPOStyleImage.createCopy(); break;
            } break;
        case Gem::NOTE:
        case Gem::TAP_ACCENT:
            switch (gemColumn)
            {
            case 0: gemImage = gemKickStyleImage.createCopy(); break;
            case 1:
            case 2:
            case 3:
            case 4:
            case 5: gemImage = gemStyleImage.createCopy(); break;
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
            case 0: gemImage = gemKickImage.createCopy(); break;
            case 1: gemImage = gemHOPOGreenImage.createCopy(); break;
            case 2: gemImage = gemHOPORedImage.createCopy(); break;
            case 3: gemImage = gemHOPOYellowImage.createCopy(); break;
            case 4: gemImage = gemHOPOBlueImage.createCopy(); break;
            case 5: gemImage = gemHOPOOrangeImage.createCopy(); break;
            } break;
        case Gem::NOTE:
        case Gem::TAP_ACCENT:
            switch (gemColumn)
            {
            case 0: gemImage = gemKickImage.createCopy(); break;
            case 1: gemImage = gemGreenImage.createCopy(); break;
            case 2: gemImage = gemRedImage.createCopy(); break;
            case 3: gemImage = gemYellowImage.createCopy(); break;
            case 4: gemImage = gemBlueImage.createCopy(); break;
            case 5: gemImage = gemOrangeImage.createCopy(); break;
            } break;
        }
    }

    return gemImage;
}


// TODO: make this actually good
// Draw measure line
// this function will create an instance of an image of a measure line, scale it (smaller as the position gets larger) and place it between the bottom and top of the window (higher up as the position gets larger) given a position between 0 and a given maximum
void ChartPreviewAudioProcessorEditor::drawMeasureLine(juce::Graphics& g, float position, bool measure)
{
    float marginTop = 0.3;
    float marginBottom = 0.25;
    float xMarginMax = 0.225;
    float xMarginMin = 0.3625;

    float yTop = getHeight() * marginTop;
    float yBottom = getHeight() * (1 - marginBottom);

    float xLeft = xMarginMax * getWidth();
    float xRight = getWidth() - xMarginMax * getWidth();




    // Position 1 Rectangle<float> (180, 450, 440, 10)

    // Position 4 Rectangle<float> (290, 180, 220, 10)
    // Henerate a rectangle based on the position and the maximum position
    juce::Rectangle<float> measureRect = juce::Rectangle<float>(
        xMarginMax * getWidth(),
        getWidth() - xMarginMax * getWidth(),
        getHeight() * (1 - marginBottom),
        10);
    // x1 = 
    // x2 = ;
    // y1 = ;
    // y2 = getHeight() * (1 - marginBottom);

    


    // float x = position / beatLines.size() * getWidth();

    // x contains the horizontal position of the line, starting at the wideLineInset and ending at the ThinLineInset from position 1 to displayedBeats
    // int xStart = (int)(xMarginMax * getWidth());
    // int xEnd = (int)(getWidth() - xMarginMax * getWidth());

    // y contains the position of the line, closer to the top up as the position gets larger. Top should be equal to displayedBeats
    // int y = (int)(position / displayedBeats * getHeight() * 0.75f);

    // float x = position / beatLines.size() * getWidth();
    // float y = measure ? 0 : getHeight() / 2;
    // float scale = measure ? 0.5f : 0.25f;

    // float y = (position / maxPosition) * (yBottom - yTop) + yTop;
    // y but

    if(measure)
    {
        g.drawImage(measureImage, measureRect);
    }
    else
    {
        g.drawImage(halfBeatImage, measureRect);
    }

    // g.drawImageAt(measureImage, juce::Rectangle<float>(xStart, y, xEnd - xStart, 10), juce::RectanglePlacement::centred);
    // g.drawImageTransformed(measureImage, juce::AffineTransform::scale(scale), juce::RectanglePlacement::centred, x, y, measureImage.getWidth(), measureImage.getHeight());
}

// // Draw beat line
// void drawBeatLine(juce::Graphics& g, int x, int y, int width, int height, int measure, int totalMeasures, int beatsPerMeasure, int beat)
// {
//     int measureWidth = width / totalMeasures;
//     int beatWidth = measureWidth / beatsPerMeasure;

//     g.setColour(juce::Colours::white);
//     g.drawLine(x + measureWidth * measure + beatWidth * beat, y, x + measureWidth * measure + beatWidth * beat, y + height, 1);
// }


// Utils
//==============================================================================
// int pWidth(int percent, int parent = 0)
// {
//     if (parent != 0)
//     {
//         return (int)(parent * (percent / 100.0));
//     }
//     else
//     {
//         return (int)(width * (percent / 100.0));
//     }
// }
// int pHeight(int percent, int parent = 0)
// {
//     if (parent != 0)
//     {
//         return (int)(parent * (percent / 100.0));
//     }
//     else
//     {
//         return (int)(height * (percent / 100.0));
//     }
// }