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

void HighwayRenderer::paint(juce::Graphics &g, PPQ trackWindowStartPPQ, PPQ trackWindowEndPPQ, PPQ displaySizeInPPQ, PPQ latencyBufferEnd)
{
    TrackWindow trackWindow = midiInterpreter.generateTrackWindow(trackWindowStartPPQ, trackWindowEndPPQ);
    SustainWindow sustainWindow = midiInterpreter.generateSustainWindow(trackWindowStartPPQ, trackWindowEndPPQ, latencyBufferEnd);
    
    // Testing with fake MIDI data
    // trackWindow = generateFakeTrackWindow(trackWindowStartPPQ, trackWindowEndPPQ);
    // trackWindow = generateFullFakeTrackWindow(trackWindowStartPPQ, trackWindowEndPPQ);
    // sustainWindow = generateFakeSustainWindow(trackWindowStartPPQ, trackWindowEndPPQ);

    // Set the drawing area dimensions from the graphics context
    auto clipBounds = g.getClipBounds();
    width = clipBounds.getWidth();
    height = clipBounds.getHeight();
    
    // Repopulate drawCallMap
    drawCallMap.clear();
    drawNotesFromMap(g, trackWindow, trackWindowStartPPQ, displaySizeInPPQ);
    drawSustainFromWindow(g, sustainWindow, trackWindowStartPPQ, displaySizeInPPQ);
    drawGridlinesFromMap(g, trackWindowStartPPQ, trackWindowEndPPQ, displaySizeInPPQ);

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

void HighwayRenderer::drawNotesFromMap(juce::Graphics &g, const TrackWindow& trackWindow, PPQ trackWindowStartPPQ, PPQ displaySizeInPPQ)
{
    for (auto &frameItem : trackWindow)
    {
        PPQ framePosition = frameItem.first;
        float normalizedPosition = (framePosition.toDouble() - trackWindowStartPPQ.toDouble()) / (float)displaySizeInPPQ.toDouble();
        drawFrame(frameItem.second, normalizedPosition, framePosition);
    }
}

void HighwayRenderer::drawGridlinesFromMap(juce::Graphics &g, PPQ trackWindowStartPPQ, PPQ trackWindowEndPPQ, PPQ displaySizeInPPQ)
{
    GridlineMap gridlineWindow = midiInterpreter.generateGridlineWindow(trackWindowStartPPQ, trackWindowEndPPQ);
    for (const auto &gridlineItem : gridlineWindow)
    {
        PPQ gridlinePPQ = gridlineItem.first;
        Gridline gridlineType = gridlineItem.second;

        float normalizedPosition = (gridlinePPQ.toDouble() - trackWindowStartPPQ.toDouble()) / displaySizeInPPQ.toDouble();

        if (normalizedPosition >= 0.0f && normalizedPosition <= 1.0f)
        {
            juce::Image *markerImage = assetManager.getGridlineImage(gridlineType);

            if (markerImage != nullptr)
            {
                drawCallMap[DrawOrder::GRID].push_back([=](juce::Graphics &g) { drawGridline(g, normalizedPosition, markerImage); });
            }
        }
    }
}

void HighwayRenderer::drawGridline(juce::Graphics& g, float position, juce::Image* markerImage)
{
    if (!markerImage) return;
    
    // Use the same positioning and dimensions as kick bars
    // For guitar, this would be column 0 (open note position)
    // For drums, this would be column 0 (kick position)
    if (isPart(state, Part::GUITAR))
    {
        // Use guitar open note positioning (column 0)
        juce::Rectangle<float> rect = getGuitarGlyphRect(0, position);
        draw(g, markerImage, rect, 1.0f);
    }
    else // if (isPart(state, Part::DRUMS))
    {
        // Use drum kick positioning (column 0)
        juce::Rectangle<float> rect = getDrumGlyphRect(0, position);
        draw(g, markerImage, rect, 1.0f);
    }
}


void HighwayRenderer::drawFrame(const std::array<Gem,LANE_COUNT> &gems, float position, PPQ framePosition)
{
    uint drawSequence[] = {0, 6, 1, 2, 3, 4, 5};
    for (int i = 0; i < gems.size(); i++)
    {
        int gemColumn = drawSequence[i];
        if (gems[gemColumn] != Gem::NONE)
        {
            drawGem(gemColumn, gems[gemColumn], position, framePosition);
        }
    }
}

void HighwayRenderer::drawGem(uint gemColumn, Gem gem, float position, PPQ framePosition)
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
    float normY1 = 0.0f, normY2 = 0.0f, normX1 = 0.0f, normX2 = 0.0f, normWidth1 = 0.0f, normWidth2 = 0.0f, scaler = 1.0f;

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
        scaler = BAR_SIZE;
    }
    else
    {
        normWidth1 = 0.12;
        normWidth2 = 0.06;
        normY1 = 0.71;
        normY2 = 0.22;
        scaler = GEM_SIZE;
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
    
    float scaledNormWidth1 = normWidth1 * scaler;
    float scaledNormWidth2 = normWidth2 * scaler;
    float adjustedNormX1 = normX1 + (normWidth1 - scaledNormWidth1) / 2.0f;
    float adjustedNormX2 = normX2 + (normWidth2 - scaledNormWidth2) / 2.0f;
    
    return createPerspectiveGlyphRect(position, normY1, normY2, adjustedNormX1, adjustedNormX2, scaledNormWidth1, scaledNormWidth2, isOpen);

}

juce::Rectangle<float> HighwayRenderer::getDrumGlyphRect(uint gemColumn, float position)
{
    float normY1 = 0.0f, normY2 = 0.0f, normX1 = 0.0f, normX2 = 0.0f, normWidth1 = 0.0f, normWidth2 = 0.0f, scaler = 1.0f;

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
        scaler = BAR_SIZE;
    }
    else
    {
        normWidth1 = 0.15;
        normWidth2 = 0.075;
        normY1 = 0.70;
        normY2 = 0.22;
        scaler = GEM_SIZE;
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
    
    float scaledNormWidth1 = normWidth1 * scaler;
    float scaledNormWidth2 = normWidth2 * scaler;
    float adjustedNormX1 = normX1 + (normWidth1 - scaledNormWidth1) / 2.0f;
    float adjustedNormX2 = normX2 + (normWidth2 - scaledNormWidth2) / 2.0f;
    
    return createPerspectiveGlyphRect(position, normY1, normY2, adjustedNormX1, adjustedNormX2, scaledNormWidth1, scaledNormWidth2, isKick);
}

juce::Rectangle<float> HighwayRenderer::getOverlayGlyphRect(Gem gem, juce::Rectangle<float> glyphRect)
{
    juce::Rectangle<float> overlayRect;
    if (isPart(state, Part::DRUMS) && gem == Gem::TAP_ACCENT)
    {
        float scaleFactor = 1.1232876712 * GEM_SIZE;
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

//==============================================================================
// Sustain Rendering

void HighwayRenderer::drawSustainFromWindow(juce::Graphics &g, const SustainWindow& sustainWindow, PPQ trackWindowStartPPQ, PPQ displaySizeInPPQ)
{
    for (const auto& sustain : sustainWindow)
    {
        drawSustain(sustain, trackWindowStartPPQ, displaySizeInPPQ);
    }
}

void HighwayRenderer::drawSustain(const SustainEvent& sustain, PPQ trackWindowStartPPQ, PPQ displaySizeInPPQ)
{
    // Calculate normalized positions for start and end of sustain
    float startPosition = (sustain.startPPQ.toDouble() - trackWindowStartPPQ.toDouble()) / displaySizeInPPQ.toDouble();
    float endPosition = (sustain.endPPQ.toDouble() - trackWindowStartPPQ.toDouble()) / displaySizeInPPQ.toDouble();
    
    // Only draw sustains that are visible in our window
    if (endPosition < 0.0f || startPosition > 1.0f) return;
    
    // Clamp to visible area
    startPosition = std::max(0.0f, startPosition);
    endPosition = std::min(1.0f, endPosition);
    
    // Get the sustain rectangle
    
    // Get sustain image based on gem column and star power state
    bool starPowerActive = state.getProperty("starPower");
    bool spNoteHeld = midiInterpreter.isNoteHeld((int)MidiPitchDefinitions::Guitar::SP, sustain.startPPQ);
    auto colour = assetManager.getLaneColour(sustain.gemColumn, isPart(state, Part::GUITAR) ? Part::GUITAR : Part::DRUMS, starPowerActive && spNoteHeld);
    // juce::Image* sustainImage = assetManager.getSustainImage(sustain.gemColumn, starPowerActive, spNoteHeld);
    // if (sustainImage == nullptr) return;
    
    // Calculate opacity (average of start and end positions for sustains)
    float avgPosition = (startPosition + endPosition) / 2.0f;
    float opacity = SUSTAIN_OPACITY * calculateOpacity(avgPosition);

    // Determine draw order - open notes (column 0) render below others
    DrawOrder sustainDrawOrder = (sustain.gemColumn == 0) ? DrawOrder::BAR : DrawOrder::SUSTAIN;
    
    // Add to draw call map
    drawCallMap[sustainDrawOrder].push_back([=](juce::Graphics &g) {
        // Draw sustain as simple flat rectangle with configurable width
        drawPerspectiveSustainFlat(g, sustain.gemColumn, startPosition, endPosition, opacity, SUSTAIN_WIDTH, colour);
    });
}

juce::Rectangle<float> HighwayRenderer::getSustainRect(uint gemColumn, float startPosition, float endPosition)
{
    // Get the rectangles using the helper function
    auto [startRect, endRect] = getSustainPositionRects(gemColumn, startPosition, endPosition);
    if (startRect.isEmpty() || endRect.isEmpty()) return juce::Rectangle<float>();
    
    // Use center points instead of full rectangle bounds for proper anchoring
    float startCenterX = startRect.getCentreX();
    float startCenterY = startRect.getCentreY(); 
    float endCenterX = endRect.getCentreX();
    float endCenterY = endRect.getCentreY();
    
    // Calculate sustain width based on the gem widths (use smaller of the two for consistency)
    float sustainWidth = std::min(startRect.getWidth(), endRect.getWidth()) * 0.8f; // Slightly narrower than gems
    
    // Create a trapezoidal sustain that follows the perspective properly
    // Top edge (closer to player, endPosition)
    float topLeft = endCenterX - sustainWidth / 2.0f;
    float topRight = endCenterX + sustainWidth / 2.0f;
    
    // Bottom edge (farther from player, startPosition)  
    float bottomLeft = startCenterX - sustainWidth / 2.0f;
    float bottomRight = startCenterX + sustainWidth / 2.0f;
    
    // For a simple Rectangle approach, we'll use the bounding box of the trapezoid
    // but ensure it follows the center line properly
    float left = std::min(topLeft, bottomLeft);
    float right = std::max(topRight, bottomRight);
    float top = endCenterY - sustainWidth * 0.1f; // Small margin above center
    float bottom = startCenterY + sustainWidth * 0.1f; // Small margin below center
    
    return juce::Rectangle<float>(left, top, right - left, bottom - top);
}

void HighwayRenderer::drawPerspectiveSustainFlat(juce::Graphics &g, uint gemColumn, float startPosition, float endPosition, float opacity, float sustainWidth, juce::Colour colour)
{
    // Get the rectangles using the helper function
    auto [startRect, endRect] = getSustainPositionRects(gemColumn, startPosition, endPosition);
    if (startRect.isEmpty() || endRect.isEmpty()) return;
    
    float startCenterX = startRect.getCentreX();
    float startCenterY = startRect.getCentreY();
    float endCenterY = endRect.getCentreY();
    
    // Calculate sustain width using the configurable width parameter
    float startSustainWidth = startRect.getWidth() * sustainWidth;
    float endSustainWidth = endRect.getWidth() * sustainWidth;
    
    // Create a trapezoidal path that follows the perspective
    juce::Path sustainPath;
    
    // Define the four corners of the trapezoid
    // Bottom edge (farther from player, startPosition)
    float bottomLeft = startCenterX - startSustainWidth / 2.0f;
    float bottomRight = startCenterX + startSustainWidth / 2.0f;
    
    // Top edge (closer to player, endPosition)
    float endCenterX = endRect.getCentreX();
    float topLeft = endCenterX - endSustainWidth / 2.0f;
    float topRight = endCenterX + endSustainWidth / 2.0f;
    
    // Build the trapezoid path
    sustainPath.startNewSubPath(bottomLeft, startCenterY);  // Bottom left
    sustainPath.lineTo(bottomRight, startCenterY);         // Bottom right
    sustainPath.lineTo(topRight, endCenterY);              // Top right  
    sustainPath.lineTo(topLeft, endCenterY);               // Top left
    sustainPath.closeSubPath();
    
    // Set opacity and fill the path with solid colors
    g.setOpacity(opacity);
    g.setColour(colour.withAlpha(opacity));
    g.fillPath(sustainPath);
}

std::pair<juce::Rectangle<float>, juce::Rectangle<float>> HighwayRenderer::getSustainPositionRects(uint gemColumn, float startPosition, float endPosition)
{
    juce::Rectangle<float> startRect, endRect;
    
    if (isPart(state, Part::DRUMS)) {
        if (gemColumn >= 5) return std::make_pair(juce::Rectangle<float>(), juce::Rectangle<float>());
        startRect = getDrumGlyphRect(gemColumn, startPosition);
        endRect = getDrumGlyphRect(gemColumn, endPosition);
    } else {
        startRect = getGuitarGlyphRect(gemColumn, startPosition);
        endRect = getGuitarGlyphRect(gemColumn, endPosition);
    }
    
    return std::make_pair(startRect, endRect);
}