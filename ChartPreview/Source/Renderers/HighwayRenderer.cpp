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

    // Draw layer by layer, then column by column within each layer
    for (const auto& drawOrder : drawCallMap)
    {
        for (const auto& column : drawOrder.second)
        {
            // Draw each layer from back to front
            for (auto it = column.second.rbegin(); it != column.second.rend(); ++it)
            {
                (*it)(g);
            }
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
                drawCallMap[DrawOrder::GRID][0].push_back([=](juce::Graphics &g) { drawGridline(g, normalizedPosition, markerImage, gridlineType); });
            }
        }
    }
}

void HighwayRenderer::drawGridline(juce::Graphics& g, float position, juce::Image* markerImage, Gridline gridlineType)
{
    if (!markerImage) return;
    
    float opacity = 1.0f;
    switch (gridlineType) {
        case Gridline::MEASURE: opacity = MEASURE_OPACITY; break;
        case Gridline::BEAT: opacity = BEAT_OPACITY; break;
        case Gridline::HALF_BEAT: opacity = HALF_BEAT_OPACITY; break;
    }

    if (isPart(state, Part::GUITAR))
    {
        juce::Rectangle<float> rect = getGuitarGridlineRect(position);
        draw(g, markerImage, rect, opacity);
    }
    else // if (isPart(state, Part::DRUMS))
    {
        juce::Rectangle<float> rect = getDrumGridlineRect(position);
        draw(g, markerImage, rect, opacity);
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
        drawCallMap[DrawOrder::BAR][gemColumn].push_back([=](juce::Graphics &g) {
            draw(g, glyphImage, glyphRect, opacity);
        });
    }
    else
    {
        drawCallMap[DrawOrder::NOTE][gemColumn].push_back([=](juce::Graphics &g) {
            draw(g, glyphImage, glyphRect, opacity);
        });
    }
    
    juce::Image* overlayImage = assetManager.getOverlayImage(gem, isPart(state, Part::GUITAR) ? Part::GUITAR : Part::DRUMS);
    if (overlayImage != nullptr)
    {
        juce::Rectangle<float> overlayRect = getOverlayGlyphRect(gem, glyphRect);

        drawCallMap[DrawOrder::OVERLAY][gemColumn].push_back([=](juce::Graphics &g) {
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
        normWidth1 = 0.125;
        normWidth2 = 0.065;
        normY1 = 0.71;
        normY2 = 0.22;
        scaler = GEM_SIZE;
        if (gemColumn == 1)
        {
            normX1 = 0.227;
            normX2 = 0.363;
            normWidth1 = 0.105;
            normWidth2 = 0.055;
        }
        else if (gemColumn == 2)
        {
            normX1 = 0.322;
            normX2 = 0.412;
        }
        else if (gemColumn == 3)
        {
            normX1 = 0.440;
            normX2 = 0.465;
        }
        else if (gemColumn == 4)
        {
            normX1 = 0.555;
            normX2 = 0.524;
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
        normWidth1 = 0.147;
        normWidth2 = 0.0714;
        normY1 = 0.70;
        normY2 = 0.22;
        scaler = GEM_SIZE;
        if (gemColumn == 1)
        {
            normX1 = 0.222;
            normX2 = 0.37;
        }
        else if (gemColumn == 2)
        {
            normX1 = 0.360;
            normX2 = 0.430;
        }
        else if (gemColumn == 3)
        {
            normX1 = 0.497;
            normX2 = 0.495;
        }
        else if (gemColumn == 4)
        {
            normX1 = 0.630;
            normX2 = 0.564;
        }
    }

    float scaledNormWidth1 = normWidth1 * scaler;
    float scaledNormWidth2 = normWidth2 * scaler;
    float adjustedNormX1 = normX1 + (normWidth1 - scaledNormWidth1) / 2.0f;
    float adjustedNormX2 = normX2 + (normWidth2 - scaledNormWidth2) / 2.0f;

    return createPerspectiveGlyphRect(position, normY1, normY2, adjustedNormX1, adjustedNormX2, scaledNormWidth1, scaledNormWidth2, isKick);
}

juce::Rectangle<float> HighwayRenderer::getGuitarGridlineRect(float position)
{
    // Use same positioning as guitar open note (column 0) but with GRIDLINE_SIZE
    float normY1 = 0.73;
    float normY2 = 0.234;
    float normX1 = 0.16;
    float normX2 = 0.34;
    float normWidth1 = 0.68;
    float normWidth2 = 0.32;
    
    float scaler = GRIDLINE_SIZE;
    bool isOpen = true;
    
    float scaledNormWidth1 = normWidth1 * scaler;
    float scaledNormWidth2 = normWidth2 * scaler;
    float adjustedNormX1 = normX1 + (normWidth1 - scaledNormWidth1) / 2.0f;
    float adjustedNormX2 = normX2 + (normWidth2 - scaledNormWidth2) / 2.0f;
    
    return createPerspectiveGlyphRect(position, normY1, normY2, adjustedNormX1, adjustedNormX2, scaledNormWidth1, scaledNormWidth2, isOpen);
}

juce::Rectangle<float> HighwayRenderer::getDrumGridlineRect(float position)
{
    // Use same positioning as drum kick note (column 0) but with GRIDLINE_SIZE
    float normY1 = 0.735;
    float normY2 = 0.239;
    float normX1 = 0.16;
    float normX2 = 0.34;
    float normWidth1 = 0.68;
    float normWidth2 = 0.32;
    
    float scaler = GRIDLINE_SIZE;
    bool isKick = true;
    
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
    
    // Calculate opacity (average of start and end positions)
    float avgPosition = (startPosition + endPosition) / 2.0f;
    float baseOpacity = calculateOpacity(avgPosition);
    
    // Lanes and sustains render differently
    float opacity, sustainWidth;
    DrawOrder sustainDrawOrder;
    switch (sustain.sustainType) {
        case SustainType::LANE:
            opacity = LANE_OPACITY * baseOpacity;
            sustainWidth = (sustain.gemColumn == 0) ? LANE_OPEN_WIDTH : LANE_WIDTH;
            sustainDrawOrder = DrawOrder::LANE;
            break;
        case SustainType::SUSTAIN:
        default:
            opacity = SUSTAIN_OPACITY * baseOpacity;
            sustainWidth = (sustain.gemColumn == 0) ? SUSTAIN_OPEN_WIDTH : SUSTAIN_WIDTH;
            sustainDrawOrder = (sustain.gemColumn == 0) ? DrawOrder::BAR : DrawOrder::SUSTAIN;
            break;
    }
    
    drawCallMap[sustainDrawOrder][sustain.gemColumn].push_back([=](juce::Graphics &g) {
        drawPerspectiveSustainFlat(g, sustain.gemColumn, startPosition, endPosition, opacity, sustainWidth, colour);
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
    // Get lane coordinates instead of glyph rectangles
    auto startLane = getLaneCoordinates(gemColumn, startPosition);
    auto endLane = getLaneCoordinates(gemColumn, endPosition);
    
    // Calculate lane widths based on sustain width parameter
    float startWidth = (startLane.rightX - startLane.leftX) * sustainWidth;
    float endWidth = (endLane.rightX - endLane.leftX) * sustainWidth;
    float radius = std::min(startWidth, endWidth) * 0.25f;
    
    // Create paths for trapezoid and rounded caps
    auto trapezoid = createTrapezoidPath(startLane, endLane, startWidth, endWidth);
    auto startCap = createRoundedCapPath(startLane, startWidth, radius);
    
    // Scale end cap height proportionally to width for natural perspective
    float endCapHeightScale = endWidth / startWidth;
    auto endCap = createRoundedCapPath(endLane, endWidth, radius, endCapHeightScale);
    
    // Render to offscreen image for perfect compositing
    auto sustainImage = createOffscreenSustainImage(trapezoid, startCap, endCap, colour);
    
    // Draw final result
    g.setOpacity(opacity);
    auto bounds = trapezoid.getBounds().getUnion(startCap.getBounds()).getUnion(endCap.getBounds());
    g.drawImageAt(sustainImage, (int)bounds.getX() - 1, (int)bounds.getY() - 1);
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

HighwayRenderer::LaneCorners HighwayRenderer::getLaneCoordinates(uint gemColumn, float position)
{
    if (isPart(state, Part::DRUMS)) {
        return getDrumLaneCoordinates(gemColumn, position);
    } else {
        return getGuitarLaneCoordinates(gemColumn, position);
    }
}

HighwayRenderer::LaneCorners HighwayRenderer::getDrumLaneCoordinates(uint gemColumn, float position)
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
        normWidth1 = 0.147;
        normWidth2 = 0.0714;
        normY1 = 0.70;
        normY2 = 0.22;
        scaler = GEM_SIZE;
        if (gemColumn == 1)
        {
            normX1 = 0.222;
            normX2 = 0.37;
        }
        else if (gemColumn == 2)
        {
            normX1 = 0.360;
            normX2 = 0.430;
        }
        else if (gemColumn == 3)
        {
            normX1 = 0.497;
            normX2 = 0.495;
        }
        else if (gemColumn == 4)
        {
            normX1 = 0.630;
            normX2 = 0.564;
        }
    }
    
    float scaledNormWidth1 = normWidth1 * scaler;
    float scaledNormWidth2 = normWidth2 * scaler;
    float adjustedNormX1 = normX1 + (normWidth1 - scaledNormWidth1) / 2.0f;
    float adjustedNormX2 = normX2 + (normWidth2 - scaledNormWidth2) / 2.0f;
    
    auto rect = createPerspectiveGlyphRect(position, normY1, normY2, adjustedNormX1, adjustedNormX2, scaledNormWidth1, scaledNormWidth2, isKick);
    return {rect.getX(), rect.getRight(), rect.getCentreY()};
}

HighwayRenderer::LaneCorners HighwayRenderer::getGuitarLaneCoordinates(uint gemColumn, float position)
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
        normWidth1 = 0.125;
        normWidth2 = 0.065;
        normY1 = 0.71;
        normY2 = 0.22;
        scaler = GEM_SIZE;
        if (gemColumn == 1)
        {
            normX1 = 0.227;
            normX2 = 0.363;
            normWidth1 = 0.105;
            normWidth2 = 0.055;
        }
        else if (gemColumn == 2)
        {
            normX1 = 0.322;
            normX2 = 0.412;
        }
        else if (gemColumn == 3)
        {
            normX1 = 0.440;
            normX2 = 0.465;
        }
        else if (gemColumn == 4)
        {
            normX1 = 0.555;
            normX2 = 0.524;
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
    
    auto rect = createPerspectiveGlyphRect(position, normY1, normY2, adjustedNormX1, adjustedNormX2, scaledNormWidth1, scaledNormWidth2, isOpen);
    return {rect.getX(), rect.getRight(), rect.getCentreY()};
}

juce::Path HighwayRenderer::createTrapezoidPath(LaneCorners start, LaneCorners end, float startWidth, float endWidth)
{
    juce::Path path;
    float startCenterX = (start.leftX + start.rightX) / 2.0f;
    float endCenterX = (end.leftX + end.rightX) / 2.0f;
    
    path.startNewSubPath(startCenterX - startWidth / 2.0f, start.centerY);
    path.lineTo(startCenterX + startWidth / 2.0f, start.centerY);
    path.lineTo(endCenterX + endWidth / 2.0f, end.centerY);
    path.lineTo(endCenterX - endWidth / 2.0f, end.centerY);
    path.closeSubPath();
    
    return path;
}

juce::Path HighwayRenderer::createRoundedCapPath(LaneCorners coords, float width, float radius, float heightScale)
{
    juce::Path path;
    float centerX = (coords.leftX + coords.rightX) / 2.0f;
    float capHeight = radius * 2.0f * heightScale;
    
    path.addRoundedRectangle(
        centerX - width / 2.0f,
        coords.centerY - capHeight / 2.0f,
        width,
        capHeight,
        radius * heightScale
    );
    
    return path;
}

juce::Image HighwayRenderer::createOffscreenSustainImage(const juce::Path& trapezoid, const juce::Path& startCap, const juce::Path& endCap, juce::Colour colour)
{
    auto bounds = trapezoid.getBounds().getUnion(startCap.getBounds()).getUnion(endCap.getBounds());
    int width = (int)std::ceil(bounds.getWidth()) + 2;
    int height = (int)std::ceil(bounds.getHeight()) + 2;
    
    juce::Image image(juce::Image::ARGB, width, height, true);
    juce::Graphics graphics(image);
    
    // Translate to local coordinates
    graphics.addTransform(juce::AffineTransform::translation(-bounds.getX() + 1, -bounds.getY() + 1));
    graphics.setColour(colour);
    
    // Draw trapezoid, then caps with clipping to avoid overlap
    graphics.fillPath(trapezoid);
    graphics.excludeClipRegion(trapezoid.getBounds().toNearestInt());
    graphics.fillPath(startCap);
    graphics.fillPath(endCap);
    
    return image;
}