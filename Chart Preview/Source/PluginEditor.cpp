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
    
}

ChartPreviewAudioProcessorEditor::~ChartPreviewAudioProcessorEditor()
{
}

void ChartPreviewAudioProcessorEditor::initState()
{
    state = juce::ValueTree("state");
    state.setProperty("skill", 4, nullptr);
    state.setProperty("part", 2, nullptr);
    state.setProperty("debug", false, nullptr);
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

    gemCymYellowImage = juce::ImageCache::getFromMemory(BinaryData::gem_cym_yellow_png, BinaryData::gem_cym_yellow_pngSize);
    gemCymBlueImage = juce::ImageCache::getFromMemory(BinaryData::gem_cym_blue_png, BinaryData::gem_cym_blue_pngSize);
    gemCymGreenImage = juce::ImageCache::getFromMemory(BinaryData::gem_cym_green_png, BinaryData::gem_cym_green_pngSize);
}

void ChartPreviewAudioProcessorEditor::initMenus()
{
    // Create menus
    skillMenu.addItemList(skillLevelLabels, 1);
    skillMenu.setSelectedId((int)SkillLevel::EXPERT, juce::NotificationType::dontSendNotification);
    skillMenu.addListener(this);
    addAndMakeVisible(skillMenu);

    partMenu.addItemList(partLabels, 1);
    partMenu.setSelectedId((int)Part::DRUMS, juce::NotificationType::dontSendNotification);
    partMenu.addListener(this);
    addAndMakeVisible(partMenu);

    drumTypeMenu.addItemList(drumTypeLabels, 1);
    drumTypeMenu.setSelectedId((int)DrumType::NORMAL, juce::NotificationType::dontSendNotification);
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
    if ((int)state.getProperty("part") == (int)Part::DRUMS)
    {
        g.drawImage(drumTrackImage, juce::Rectangle<float>(0, 0, getWidth(), getHeight()), juce::RectanglePlacement::centred);
    }
    else if ((int)state.getProperty("part") == (int)Part::GUITAR)
    {
        g.drawImage(guitarTrackImage, juce::Rectangle<float>(0, 0, getWidth(), getHeight()), juce::RectanglePlacement::centred);
    }

    drumTypeMenu.setVisible((int)state.getProperty("part") == (int)Part::DRUMS);
    kick2xToggle.setVisible((int)state.getProperty("part") == (int)Part::DRUMS);
    dynamicsToggle.setVisible((int)state.getProperty("part") == (int)Part::DRUMS);
    
    consoleOutput.setVisible(debugToggle.getToggleState());

    // // Draw lines
    // for (int i = 0; i < beatLines.size(); i++)
    // {
    //     drawMeasureLine(g, beatLines[i].position, beatLines[i].measure);
    // }

    // Draw Buffer
    auto midiEventMap = audioProcessor.getMidiEventMap();
    auto noteStateMap = audioProcessor.getNoteStateMap();
    // auto midiEventMap = midiInterpreter.getFakeMidiEventMap();

    int lower = std::max(0, currentPlayheadPositionInSamples() - 1);
    int upper = currentPlayheadPositionInSamples() + displaySizeInSamples;
    auto lowerMEM = midiEventMap.lower_bound(lower);
    auto upperMEM = midiEventMap.upper_bound(upper);
    for (auto it = lowerMEM; it != upperMEM; ++it)
    {
        const int positionInSamples = it->first;
        float normalizedPosition = (positionInSamples - currentPlayheadPositionInSamples()) / (float)displaySizeInSamples;
        std::vector<uint> gems = midiInterpreter.interpretMidiFrameOLD(it->second);
        std::array<Gem,7> gemsToRender = midiInterpreter.interpretMidiFrame(it->second, noteStateMap[positionInSamples]);

        // TODO: Find distance from previous note to see if auto HOPO

        // TODO: Find distance to next note off to see if sustain

        drawGemGroup(g, gems, normalizedPosition);
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

    // debugToggle.setBounds(340, 10, 100, 20);

    consoleOutput.setBounds(10, 40, getWidth() - 20, getHeight() - 50);
}

// Draw Functions
//==============================================================================

void ChartPreviewAudioProcessorEditor::drawGemGroup(juce::Graphics &g, const std::vector<uint> &gems, float position)
{
    for(int i = 0; i < gems.size(); i++)
    {
        if (gems[i] > 0)
        {
            drawGem(g, i, gems[i], position);
        }
    }
}

void ChartPreviewAudioProcessorEditor::drawGem(juce::Graphics& g, uint gemColumn, uint gemType, float position)
{
    int instrument = partMenu.getSelectedId();

    // Relative placement and sizes of gems from position 0.0 to 1.0
    float normY1, normY2, normX1, normX2, normWidth1, normWidth2;

    if (instrument == 1)
    {
        normWidth1 = 0.10, normWidth2 = 0.050;
        normY1 = 0.71, normY2 = 0.22;
        if (gemColumn == 0)
        {
            normX1 = 0.22;
            normX2 = 0.365;
        }
        else if (gemColumn == 1)
        {
            normX1 = 0.332;
            normX2 = 0.420;
        }
        else if (gemColumn == 2)
        {
            normX1 = 0.450;
            normX2 = 0.475;
        }
        else if (gemColumn == 3)
        {
            normX1 = 0.568;
            normX2 = 0.532;
        }
        else if (gemColumn == 4)
        {
            normX1 = 0.680;
            normX2 = 0.590;
        }
    }
    else if (instrument == 2)
    {
        if (gemColumn == 0)
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
    }

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
    if (instrument == 2 && gemColumn == 0)
    {
        height = (int) width / 16.f;
    }
    else
    {
        height = (int) width / 2.f;   
    }

    juce::Rectangle<float> glyphRect = juce::Rectangle<float>(xPos, yPos, width, height);

    // Get proper glyph
    juce::Image glyphImage;


    if (instrument == 1)
    {
        if (gemColumn == 0){ glyphImage = gemGreenImage.createCopy(); }
        else if (gemColumn == 1){ glyphImage = gemRedImage.createCopy(); }
        else if (gemColumn == 2){ glyphImage = gemYellowImage.createCopy(); }
        else if (gemColumn == 3){ glyphImage = gemBlueImage.createCopy(); }
        else if (gemColumn == 4){ glyphImage = gemOrangeImage.createCopy(); }
    }

    if (instrument == 2)
    {
        if (gemColumn == 0) { 
            glyphImage = gemKickImage.createCopy(); 
        }
        else
        {
            // Ghost drum
            if (gemType == 1)
            {
                if (gemColumn == 1){ glyphImage = gemRedImage.createCopy(); }
                else if (gemColumn == 2){ glyphImage = gemYellowImage.createCopy(); }
                else if (gemColumn == 3){ glyphImage = gemBlueImage.createCopy(); }
                else if (gemColumn == 4){ glyphImage = gemGreenImage.createCopy(); }
                glyphImage.multiplyAllAlphas(0.5);
            }
            // Normal drum
            else if (gemType == 2)
            {
                if (gemColumn == 1){ glyphImage = gemRedImage.createCopy(); }
                else if (gemColumn == 2){ glyphImage = gemYellowImage.createCopy(); }
                else if (gemColumn == 3){ glyphImage = gemBlueImage.createCopy(); }
                else if (gemColumn == 4){ glyphImage = gemGreenImage.createCopy(); }
            }
            // Accent drum
            else if (gemType == 3)
            {
                if (gemColumn == 1){ glyphImage = gemRedImage.createCopy(); }
                else if (gemColumn == 2){ glyphImage = gemYellowImage.createCopy(); }
                else if (gemColumn == 3){ glyphImage = gemBlueImage.createCopy(); }
                else if (gemColumn == 4){ glyphImage = gemGreenImage.createCopy(); }
            }
            // Ghost cymbal
            else if (gemType == 4)
            {
                if (gemColumn == 2){ glyphImage = gemCymYellowImage.createCopy(); }
                else if (gemColumn == 3){ glyphImage = gemCymBlueImage.createCopy(); }
                else if (gemColumn == 4){ glyphImage = gemCymGreenImage.createCopy(); }
                glyphImage.multiplyAllAlphas(0.5);
            }
            // Normal cymbal
            else if (gemType == 5)
            {
                if (gemColumn == 2){ glyphImage = gemCymYellowImage.createCopy(); }
                else if (gemColumn == 3){ glyphImage = gemCymBlueImage.createCopy(); }
                else if (gemColumn == 4){ glyphImage = gemCymGreenImage.createCopy(); }
            }
            // Accent cymbal
            else if (gemType == 6)
            {
                if (gemColumn == 2){ glyphImage = gemCymYellowImage.createCopy(); }
                else if (gemColumn == 3){ glyphImage = gemCymBlueImage.createCopy(); }
                else if (gemColumn == 4){ glyphImage = gemCymGreenImage.createCopy(); }
            }
        }
    }

    // Opacity
    float opacity;
    float opacityStart = 0.9;
    if (position >= opacityStart)
    {
        float opacity = 1.0 - ((position - opacityStart) / (1.0 - opacityStart));
        glyphImage.multiplyAllAlphas(opacity);
    }

    g.drawImage(glyphImage, glyphRect);
}

// void ChartPreviewAudioProcessorEditor::drawDrumGem(juce::Graphics &g, uint gemColumn, uint gemType, float position)
// {
//     repaint();
// }

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