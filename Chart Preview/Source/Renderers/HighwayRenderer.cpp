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
	  midiInterpreter(midiInterpreter),
	  assetManager()
{
}

HighwayRenderer::~HighwayRenderer()
{
}

void HighwayRenderer::paint(juce::Graphics &g, PPQPosition trackWindowStartPPQ, PPQPosition trackWindowEndPPQ, PPQPosition displaySizeInPPQ)
{
	TrackWindow trackWindow = midiInterpreter.generateTrackWindow(trackWindowStartPPQ, trackWindowEndPPQ);

	// // FAKE DATA
	// TrackWindow trackWindow;
    // trackWindow[trackWindowStartPPQ + (1.0 * displaySizeInPPQ / 7)] = {Gem::NOTE, Gem::HOPO_GHOST, Gem::HOPO_GHOST, Gem::HOPO_GHOST, Gem::HOPO_GHOST, Gem::NONE, Gem::NONE};
	// trackWindow[trackWindowStartPPQ + (2.0 * displaySizeInPPQ / 7)] = {Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE, Gem::NOTE};
	// trackWindow[trackWindowStartPPQ + (2.0 * displaySizeInPPQ / 7)] = {Gem::NONE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NOTE, Gem::NONE, Gem::NONE};
	// trackWindow[trackWindowStartPPQ + (3.0 * displaySizeInPPQ / 7)] = {Gem::NONE, Gem::TAP_ACCENT, Gem::TAP_ACCENT, Gem::TAP_ACCENT, Gem::TAP_ACCENT, Gem::NONE, Gem::NONE};
	// trackWindow[trackWindowStartPPQ + (4.0 * displaySizeInPPQ / 7)] = {Gem::NONE, Gem::NONE, Gem::CYM_GHOST, Gem::CYM_GHOST, Gem::CYM_GHOST, Gem::NONE, Gem::NONE};
	// trackWindow[trackWindowStartPPQ + (5.0 * displaySizeInPPQ / 7)] = {Gem::NONE, Gem::NONE, Gem::CYM, Gem::CYM, Gem::CYM, Gem::NONE, Gem::NONE};
	// trackWindow[trackWindowStartPPQ + (6.0 * displaySizeInPPQ / 7)] = {Gem::NONE, Gem::NONE, Gem::CYM_ACCENT, Gem::CYM_ACCENT, Gem::CYM_ACCENT, Gem::NONE, Gem::NONE};

    drawCallMap.clear();

    // Set the drawing area dimensions from the graphics context
    auto clipBounds = g.getClipBounds();
    width = clipBounds.getWidth();
    height = clipBounds.getHeight();

    // Populate drawCallMap
    for (auto &frameItem : trackWindow)
	{
		framePosition = frameItem.first;
		float normalizedPosition = (framePosition.toDouble() - trackWindowStartPPQ.toDouble()) / (float)displaySizeInPPQ.toDouble();
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

    // Draw gridlines using the same mechanism as kick bars
    drawGridlines(g, trackWindowStartPPQ, trackWindowEndPPQ, displaySizeInPPQ);
}



void HighwayRenderer::drawGridlines(juce::Graphics& g, PPQPosition trackWindowStartPPQ, PPQPosition trackWindowEndPPQ, PPQPosition displaySizeInPPQ)
{
    
}

void HighwayRenderer::drawMeterBar(juce::Graphics& g, float position, juce::Image* markerImage)
{
    if (!markerImage) return;
    
    // Use the same positioning and dimensions as kick bars
    // For guitar, this would be column 0 (open note position)
    // For drums, this would be column 0 (kick position)
    Part part = (Part)((int)state.getProperty("part"));
    
    if (part == Part::GUITAR)
    {
        // Use guitar open note positioning (column 0)
        juce::Rectangle<float> rect = getGuitarGlyphRect(0, position);
        draw(g, markerImage, rect, 1.0f);
    }
    else
    {
        // Use drum kick positioning (column 0)
        juce::Rectangle<float> rect = getDrumGlyphRect(0, position);
        draw(g, markerImage, rect, 1.0f);
    }
}


void HighwayRenderer::drawFrame(const std::array<Gem,LANE_COUNT> &gems, float position)
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
        bool starPowerActive = state.getProperty("starPower");
        bool spNoteHeld = midiInterpreter.isNoteHeld((int)MidiPitchDefinitions::Guitar::SP, framePosition);
        glyphImage = assetManager.getGuitarGlyphImage(gem, gemColumn, starPowerActive, spNoteHeld);
        barNote = isBarNote(gemColumn, Part::GUITAR);
    }
    else // if (isPart(state, Part::DRUMS))
    {
        glyphRect = getDrumGlyphRect(gemColumn, position);
        bool starPowerActive = state.getProperty("starPower");
        bool spNoteHeld = midiInterpreter.isNoteHeld((int)MidiPitchDefinitions::Drums::SP, framePosition);
        glyphImage = assetManager.getDrumGlyphImage(gem, gemColumn, starPowerActive, spNoteHeld);
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
    
    juce::Image* overlayImage = assetManager.getOverlayImage(gem, isPart(state, Part::GUITAR) ? Part::GUITAR : Part::DRUMS);
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

juce::Rectangle<float> HighwayRenderer::getGuitarGlyphRect(uint gemColumn, float position)
{
    float normY1 = 0.0f, normY2 = 0.0f, normX1 = 0.0f, normX2 = 0.0f, normWidth1 = 0.0f, normWidth2 = 0.0f;

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
        normWidth1 = 0.12;
        normWidth2 = 0.06;
        normY1 = 0.71;
        normY2 = 0.22;
        if (gemColumn == 1)
        {
            normX1 = 0.21;
            normX2 = 0.360;
        }
        else if (gemColumn == 2)
        {
            normX1 = 0.322;
            normX2 = 0.410;
        }
        else if (gemColumn == 3)
        {
            normX1 = 0.440;
            normX2 = 0.465;
        }
        else if (gemColumn == 4)
        {
            normX1 = 0.558;
            normX2 = 0.522;
        }
        else if (gemColumn == 5)
        {
            normX1 = 0.670;
            normX2 = 0.580;
        }
    }

    return createPerspectiveGlyphRect(position, normY1, normY2, normX1, normX2, normWidth1, normWidth2, isOpen);
}

juce::Rectangle<float> HighwayRenderer::getDrumGlyphRect(uint gemColumn, float position)
{
    float normY1 = 0.0f, normY2 = 0.0f, normX1 = 0.0f, normX2 = 0.0f, normWidth1 = 0.0f, normWidth2 = 0.0f;

    // If the gem is a kick
    bool isKick = isBarNote(gemColumn, Part::DRUMS);
    if (isKick)
    {
        normY1 = 0.735;
        normY2 = 0.239;
        normX1 = 0.16;
        normX2 = 0.34;
        normWidth1 = 0.68;
        normWidth2 = 0.32;
    }
    else
    {
        normWidth1 = 0.15;
        normWidth2 = 0.075;
        normY1 = 0.70;
        normY2 = 0.22;
        if (gemColumn == 1)
        {
            normX1 = 0.21;
            normX2 = 0.360;
        }
        else if (gemColumn == 2)
        {
            normX1 = 0.352;
            normX2 = 0.424;
        }
        else if (gemColumn == 3)
        {
            normX1 = 0.497;
            normX2 = 0.494;
        }
        else if (gemColumn == 4)
        {
            normX1 = 0.640;
            normX2 = 0.560;
        }
    }

    return createPerspectiveGlyphRect(position, normY1, normY2, normX1, normX2, normWidth1, normWidth2, isKick);
}

juce::Rectangle<float> HighwayRenderer::getOverlayGlyphRect(Gem gem, juce::Rectangle<float> glyphRect)
{
    juce::Rectangle<float> overlayRect;
    if (isPart(state, Part::DRUMS) && gem == Gem::TAP_ACCENT)
    {
        float scaleFactor = 1.1232876712;
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
// 3D Perspective Highway Rendering

juce::Rectangle<float> HighwayRenderer::createPerspectiveGlyphRect(float position, float normY1, float normY2, float normX1, float normX2, float normWidth1, float normWidth2, bool isBarNote)
{
    // 3D perspective parameters
    const float highwayDepth = 100.0f;
    const float playerDistance = 50.0f;
    const float perspectiveStrength = 0.7f;
    const float exponentialCurve = 0.5f;
    const float xOffsetMultiplier = 0.5f;
    const float barNoteHeightRatio = 16.0f;
    const float regularNoteHeightRatio = 2.0f;

    float depth = position;
    
    // Calculate 3D perspective scale for height
    float perspectiveScale = (playerDistance + highwayDepth * (1.0f - depth)) / playerDistance;
    perspectiveScale = 1.0f + (perspectiveScale - 1.0f) * perspectiveStrength;
    
    // Calculate dimensions
    float targetWidth = normWidth2 * width;
    float targetHeight = isBarNote ? targetWidth / barNoteHeightRatio : targetWidth / regularNoteHeightRatio;
    
    // Width calculation: both note types use exponential interpolation
    float widthProgress = (std::pow(10, exponentialCurve * (1 - depth)) - 1) / (std::pow(10, exponentialCurve) - 1);
    float interpolatedWidth = normWidth2 + (normWidth1 - normWidth2) * widthProgress;
    float finalWidth = interpolatedWidth * width;
    
    // Height uses perspective scaling for 3D effect
    float currentHeight = targetHeight * perspectiveScale;
    
    // Position calculation using exponential curve
    float progress = (std::pow(10, exponentialCurve * (1 - depth)) - 1) / (std::pow(10, exponentialCurve) - 1);
    float yPos = normY2 * height + (normY1 - normY2) * height * progress;
    float xPos = normX2 * width + (normX1 - normX2) * width * progress;
    
    // Apply X offset and center positioning
    float xOffset = targetWidth * xOffsetMultiplier;
    float finalX = xPos + xOffset - targetWidth / 2.0f;
    float finalY = yPos - targetHeight / 2.0f;
    
    return juce::Rectangle<float>(finalX, finalY, finalWidth, currentHeight);
}
