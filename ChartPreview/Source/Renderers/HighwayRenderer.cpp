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

void HighwayRenderer::paint(juce::Graphics &g, const TimeBasedTrackWindow& trackWindow, const TimeBasedSustainWindow& sustainWindow, const TimeBasedGridlineMap& gridlines, double windowStartTime, double windowEndTime, bool isPlaying)
{
    // Set the drawing area dimensions from the graphics context
    auto clipBounds = g.getClipBounds();
    width = clipBounds.getWidth();
    height = clipBounds.getHeight();

    // Calculate the total time window
    double windowTimeSpan = windowEndTime - windowStartTime;

    // Update sustain states for active animations
    updateSustainStates(sustainWindow);

    // Repopulate drawCallMap
    drawCallMap.clear();
    drawNotesFromMap(g, trackWindow, windowStartTime, windowEndTime);
    drawSustainFromWindow(g, sustainWindow, windowStartTime, windowEndTime);
    drawGridlinesFromMap(g, gridlines, windowStartTime, windowEndTime);

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

    // Draw hit animations on top of everything (if enabled)
    bool hitIndicatorsEnabled = state.getProperty("hitIndicators");
    if (hitIndicatorsEnabled)
    {
        if (isPlaying){ detectAndTriggerHitAnimations(trackWindow, windowStartTime, windowEndTime); }
        drawHitAnimations(g);
        hitAnimationManager.advanceAllFrames();
    }
}

void HighwayRenderer::drawNotesFromMap(juce::Graphics &g, const TimeBasedTrackWindow& trackWindow, double windowStartTime, double windowEndTime)
{
    double windowTimeSpan = windowEndTime - windowStartTime;

    for (const auto &frameItem : trackWindow)
    {
        double frameTime = frameItem.first;  // Time in seconds from cursor

        // Don't render notes in the past (below the strikeline at time 0)
        if (frameTime < 0.0) continue;

        // Normalize position: 0 = far (window start), 1 = near (window end/strikeline)
        float normalizedPosition = (float)((frameTime - windowStartTime) / windowTimeSpan);

        drawFrame(frameItem.second, normalizedPosition, frameTime);
    }
}

void HighwayRenderer::drawGridlinesFromMap(juce::Graphics &g, const TimeBasedGridlineMap& gridlines, double windowStartTime, double windowEndTime)
{
    double windowTimeSpan = windowEndTime - windowStartTime;

    for (const auto &gridline : gridlines)
    {
        double gridlineTime = gridline.time;  // Time in seconds from cursor
        Gridline gridlineType = gridline.type;

        // Normalize position: 0 = far (window start), 1 = near (window end/strikeline)
        float normalizedPosition = (float)((gridlineTime - windowStartTime) / windowTimeSpan);

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


void HighwayRenderer::drawFrame(const TimeBasedTrackFrame &gems, float position, double frameTime)
{
    uint drawSequence[] = {0, 6, 1, 2, 3, 4, 5};
    for (int i = 0; i < gems.size(); i++)
    {
        int gemColumn = drawSequence[i];
        if (gems[gemColumn] != Gem::NONE)
        {
            drawGem(gemColumn, gems[gemColumn], position, frameTime);
        }
    }
}

void HighwayRenderer::drawGem(uint gemColumn, Gem gem, float position, double frameTime)
{
    juce::Rectangle<float> glyphRect;
    juce::Image* glyphImage;
    bool barNote;

    if (isPart(state, Part::GUITAR))
    {
        glyphRect = getGuitarGlyphRect(gemColumn, position);
        bool starPowerActive = state.getProperty("starPower");
        // TODO: Need to convert frameTime back to PPQ for star power check, or pass SP state differently
        bool spNoteHeld = false; // Temporarily disabled
        glyphImage = assetManager.getGuitarGlyphImage(gem, gemColumn, starPowerActive, spNoteHeld);
        barNote = isBarNote(gemColumn, Part::GUITAR);
    }
    else // if (isPart(state, Part::DRUMS))
    {
        glyphRect = getDrumGlyphRect(gemColumn, position);
        bool starPowerActive = state.getProperty("starPower");
        // TODO: Need to convert frameTime back to PPQ for star power check, or pass SP state differently
        bool spNoteHeld = false; // Temporarily disabled
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
        normY1 = 0.745;
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
        normY1 = 0.73;
        normY2 = 0.22;
        scaler = GEM_SIZE;
        if (gemColumn == 1)
        {
            normX1 = 0.20;
            normX2 = 0.363;
        }
        else if (gemColumn == 2)
        {
            normX1 = 0.320;
            normX2 = 0.412;
        }
        else if (gemColumn == 3)
        {
            normX1 = 0.440;
            normX2 = 0.465;
        }
        else if (gemColumn == 4)
        {
            normX1 = 0.557;
            normX2 = 0.524;
        }
        else if (gemColumn == 5)
        {
            normX1 = 0.673;
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
        normY1 = 0.75;
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
        normY1 = 0.72;
        normY2 = 0.22;
        scaler = GEM_SIZE;
        if (gemColumn == 1)
        {
            normX1 = 0.22;
            normX2 = 0.365;
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
            normX1 = 0.640;
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

void HighwayRenderer::drawSustainFromWindow(juce::Graphics &g, const TimeBasedSustainWindow& sustainWindow, double windowStartTime, double windowEndTime)
{
    for (const auto& sustain : sustainWindow)
    {
        drawSustain(sustain, windowStartTime, windowEndTime);
    }
}

void HighwayRenderer::drawSustain(const TimeBasedSustainEvent& sustain, double windowStartTime, double windowEndTime)
{
    double windowTimeSpan = windowEndTime - windowStartTime;

    // Don't render sustains that end before the strikeline (time 0)
    if (sustain.endTime < 0.0) return;

    // Clip sustain start to the strikeline if it extends into the past
    double clippedStartTime = std::max(0.0, sustain.startTime);

    // Calculate normalized positions for start and end of sustain
    float startPosition = (float)((clippedStartTime - windowStartTime) / windowTimeSpan);
    float endPosition = (float)((sustain.endTime - windowStartTime) / windowTimeSpan);

    // Only draw sustains that are visible in our window
    if (endPosition < 0.0f || startPosition > 1.0f) return;

    // Clamp to visible area
    startPosition = std::max(0.0f, startPosition);
    endPosition = std::min(1.0f, endPosition);

    // Get sustain image based on gem column and star power state
    bool starPowerActive = state.getProperty("starPower");
    // TODO: Need to pass SP state differently for time-based rendering
    bool spNoteHeld = false; // Temporarily disabled
    auto colour = assetManager.getLaneColour(sustain.gemColumn, isPart(state, Part::GUITAR) ? Part::GUITAR : Part::DRUMS, starPowerActive && spNoteHeld);

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

//==============================================================================
// Hit Animation Detection & Rendering

void HighwayRenderer::detectAndTriggerHitAnimations(const TimeBasedTrackWindow& trackWindow, double windowStartTime, double windowEndTime)
{
    // Strikeline is at time 0 (current playback position)
    // For each column, find the closest note that has passed the strikeline
    // If it's a new note (different from last frame), trigger the animation

    std::array<double, 7> closestPastNotePerColumn = {999.0, 999.0, 999.0, 999.0, 999.0, 999.0, 999.0};

    // Find the closest note that has just crossed (or is at) the strikeline for each column
    for (const auto &frameItem : trackWindow)
    {
        double frameTime = frameItem.first;  // Time in seconds from cursor
        const auto& gems = frameItem.second;

        // We only care about notes that have crossed or are at the strikeline (frameTime <= 0)
        // And are close enough to be considered "just hit" (within a small past window)
        if (frameTime <= 0.0 && frameTime >= -0.05)  // 50ms past window
        {
            for (uint gemColumn = 0; gemColumn < gems.size(); ++gemColumn)
            {
                if (gems[gemColumn] != Gem::NONE)
                {
                    // This note is past the strikeline - check if it's the closest one
                    if (std::abs(frameTime) < std::abs(closestPastNotePerColumn[gemColumn]))
                    {
                        closestPastNotePerColumn[gemColumn] = frameTime;
                    }
                }
            }
        }
    }

    // Now trigger animations for any column where we found a new note
    for (uint gemColumn = 0; gemColumn < closestPastNotePerColumn.size(); ++gemColumn)
    {
        // If we found a note (not 999.0) and it's different from the last one we processed
        if (closestPastNotePerColumn[gemColumn] < 999.0 &&
            closestPastNotePerColumn[gemColumn] != lastNoteTimePerColumn[gemColumn])
        {
            // This is a new note! Trigger the animation
            lastNoteTimePerColumn[gemColumn] = closestPastNotePerColumn[gemColumn];

            if (isPart(state, Part::GUITAR))
            {
                if (gemColumn == 0) {
                    // Open note (kick for guitar)
                    hitAnimationManager.triggerKick(true, false);
                } else if (gemColumn >= 1 && gemColumn <= 5) {
                    // Regular fret (1=green, 2=red, 3=yellow, 4=blue, 5=orange)
                    hitAnimationManager.triggerHit(gemColumn, false);
                }
            }
            else // Part::DRUMS
            {
                if (gemColumn == 0) {
                    // Regular kick
                    hitAnimationManager.triggerKick(false, false);
                } else if (gemColumn == 6) {
                    // 2x kick
                    hitAnimationManager.triggerKick(false, true);
                } else if (gemColumn >= 1 && gemColumn <= 4) {
                    // Drum pads
                    hitAnimationManager.triggerHit(gemColumn, false);
                }
            }
        }
    }
}

void HighwayRenderer::updateSustainStates(const TimeBasedSustainWindow& sustainWindow)
{
    // Strikeline is at time 0 (current playback position)
    // Check if each lane is currently in a sustain (sustain crosses the strikeline)
    std::array<bool, 6> lanesSustaining = {false, false, false, false, false, false};

    for (const auto& sustain : sustainWindow)
    {
        // Sustain is active at the strikeline if startTime <= 0 <= endTime
        if (sustain.startTime <= 0.0 && sustain.endTime >= 0.0)
        {
            // Only track sustains (not lanes)
            if (sustain.sustainType == SustainType::SUSTAIN && sustain.gemColumn < lanesSustaining.size())
            {
                lanesSustaining[sustain.gemColumn] = true;
            }
        }
    }

    // Update sustain state for each lane
    for (size_t lane = 0; lane < lanesSustaining.size(); ++lane)
    {
        hitAnimationManager.setSustainState(static_cast<int>(lane), lanesSustaining[lane]);
    }
}

void HighwayRenderer::drawHitAnimations(juce::Graphics &g)
{
    const auto& animations = hitAnimationManager.getActiveAnimations();

    // Strikeline is where notes are when frameTime = 0 (at the cursor position)
    float strikelinePosition = 0.0f;

    for (const auto& anim : animations)
    {
        if (!anim.isActive()) continue;

        if (anim.isBar)
        {
            // Draw bar animation at bar position (gemColumn 0 for open/kick, or 6 for 2x kick)
            auto kickFrame = assetManager.getKickAnimationFrame(anim.currentFrame);
            if (kickFrame)
            {
                juce::Rectangle<float> kickRect;
                if (isPart(state, Part::GUITAR)) {
                    kickRect = getGuitarGlyphRect(0, strikelinePosition);
                } else {
                    kickRect = getDrumGlyphRect(anim.is2xKick ? 6 : 0, strikelinePosition);
                }

                // Scale up the animation (wider and MUCH taller to match the bar note height)
                kickRect = kickRect.withSizeKeepingCentre(kickRect.getWidth() * 1.3f, kickRect.getHeight() * 4.2f);

                g.setOpacity(1.0f);

                // Apply purple tint for open notes on guitar
                if (isPart(state, Part::GUITAR))
                {
                    // Create a purple-tinted version of the image
                    juce::Colour purpleTint = juce::Colour(180, 120, 220);

                    // Draw with color overlay using ColourGradient or direct tinting
                    g.setColour(purpleTint);
                    g.setOpacity(0.5f);
                    g.drawImage(*kickFrame, kickRect, juce::RectanglePlacement::stretchToFit, false);
                    g.setOpacity(1.0f);
                    g.drawImage(*kickFrame, kickRect, juce::RectanglePlacement::stretchToFit, false);
                }
                else
                {
                    g.drawImage(*kickFrame, kickRect);
                }
            }
        }
        else
        {
            // Draw fret hit animation (flash + flare)
            auto hitFrame = assetManager.getHitAnimationFrame(anim.currentFrame);
            Part currentPart = isPart(state, Part::GUITAR) ? Part::GUITAR : Part::DRUMS;
            auto flareImage = assetManager.getHitFlareImage(anim.lane, currentPart);

            juce::Rectangle<float> hitRect;
            if (currentPart == Part::GUITAR) {
                hitRect = getGuitarGlyphRect(anim.lane, strikelinePosition);
            } else {
                hitRect = getDrumGlyphRect(anim.lane, strikelinePosition);
            }

            // Scale up the animation (wider and much taller)
            hitRect = hitRect.withSizeKeepingCentre(hitRect.getWidth() * 1.6f, hitRect.getHeight() * 2.8f);

            // Draw the flash frame
            if (hitFrame)
            {
                g.setOpacity(0.8f);
                g.drawImage(*hitFrame, hitRect);
            }

            // Draw the colored flare on top (with tint for the lane color)
            if (flareImage && anim.currentFrame <= 3)  // Only show flare for first 3 frames
            {
                g.setOpacity(0.6f);
                g.drawImage(*flareImage, hitRect);
            }
        }
    }
}