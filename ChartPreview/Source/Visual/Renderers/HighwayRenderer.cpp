/*
  ==============================================================================

    HighwayRenderer.cpp
    Created: 15 Jun 2024 3:57:32pm
    Author:  Noah Baxter

  ==============================================================================
*/

#include "HighwayRenderer.h"
#include "../Utils/DrawingConstants.h"
#include "../Utils/PositionConstants.h"

using namespace PositionConstants;

HighwayRenderer::HighwayRenderer(juce::ValueTree &state, MidiInterpreter &midiInterpreter)
	: state(state),
	  midiInterpreter(midiInterpreter),
	  assetManager(),
	  animationRenderer(state, midiInterpreter)
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

    // Update sustain states for active animations (force-trigger if playing into active sustain)
    animationRenderer.updateSustainStates(sustainWindow, isPlaying);

    // Clear animations when paused to indicate gem is not being played
    if (!isPlaying)
    {
        animationRenderer.reset();
    }

    // Repopulate drawCallMap
    drawCallMap.clear();
    drawNotesFromMap(g, trackWindow, windowStartTime, windowEndTime);
    drawSustainFromWindow(g, sustainWindow, windowStartTime, windowEndTime);
    drawGridlinesFromMap(g, gridlines, windowStartTime, windowEndTime);

    // Detect and add animations to drawCallMap (if enabled)
    bool hitIndicatorsEnabled = state.getProperty("hitIndicators");
    if (hitIndicatorsEnabled)
    {
        if (isPlaying) { animationRenderer.detectAndTriggerAnimations(trackWindow); }
        animationRenderer.renderToDrawCallMap(drawCallMap, width, height);
    }

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

    // Advance animation frames after rendering
    if (hitIndicatorsEnabled)
    {
        animationRenderer.advanceFrames();
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
        juce::Rectangle<float> rect = glyphRenderer.getGuitarGridlineRect(position, width, height);
        draw(g, markerImage, rect, opacity);
    }
    else // if (isPart(state, Part::DRUMS))
    {
        juce::Rectangle<float> rect = glyphRenderer.getDrumGridlineRect(position, width, height);
        draw(g, markerImage, rect, opacity);
    }
}


void HighwayRenderer::drawFrame(const TimeBasedTrackFrame &gems, float position, double frameTime)
{
    uint drawSequence[] = {0, 6, 1, 2, 3, 4, 5};
    for (int i = 0; i < gems.size(); i++)
    {
        int gemColumn = drawSequence[i];
        if (gems[gemColumn].gem != Gem::NONE)
        {
            drawGem(gemColumn, gems[gemColumn], position, frameTime);
        }
    }
}

void HighwayRenderer::drawGem(uint gemColumn, const GemWrapper& gemWrapper, float position, double frameTime)
{
    juce::Rectangle<float> glyphRect;
    juce::Image* glyphImage;
    bool barNote;

    if (isPart(state, Part::GUITAR))
    {
        glyphRect = glyphRenderer.getGuitarGlyphRect(gemColumn, position, width, height);
        bool starPowerActive = state.getProperty("starPower");
        glyphImage = assetManager.getGuitarGlyphImage(gemWrapper, gemColumn, starPowerActive);
        barNote = isBarNote(gemColumn, Part::GUITAR);
    }
    else // if (isPart(state, Part::DRUMS))
    {
        glyphRect = glyphRenderer.getDrumGlyphRect(gemColumn, position, width, height);
        bool starPowerActive = state.getProperty("starPower");
        glyphImage = assetManager.getDrumGlyphImage(gemWrapper, gemColumn, starPowerActive);
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

    juce::Image* overlayImage = assetManager.getOverlayImage(gemWrapper.gem, isPart(state, Part::GUITAR) ? Part::GUITAR : Part::DRUMS);
    if (overlayImage != nullptr)
    {
        bool isDrumAccent = !isPart(state, Part::GUITAR) && gemWrapper.gem == Gem::TAP_ACCENT;
        juce::Rectangle<float> overlayRect = glyphRenderer.getOverlayGlyphRect(glyphRect, isDrumAccent);

        drawCallMap[DrawOrder::OVERLAY][gemColumn].push_back([=](juce::Graphics &g) {
            draw(g, overlayImage, overlayRect, opacity);
        });
    }
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

    // Get sustain color based on gem column and star power state
    bool starPowerActive = state.getProperty("starPower");
    bool shouldBeWhite = starPowerActive && sustain.gemType.starPower;
    auto colour = assetManager.getLaneColour(sustain.gemColumn, isPart(state, Part::GUITAR) ? Part::GUITAR : Part::DRUMS, shouldBeWhite);

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

void HighwayRenderer::drawPerspectiveSustainFlat(juce::Graphics &g, uint gemColumn, float startPosition, float endPosition, float opacity, float sustainWidth, juce::Colour colour)
{
    // Get lane coordinates instead of glyph rectangles
    auto startLane = isPart(state, Part::DRUMS) ? PositionMath::getDrumLaneCoordinates(gemColumn, startPosition, width, height) : PositionMath::getGuitarLaneCoordinates(gemColumn, startPosition, width, height);
    auto endLane = isPart(state, Part::DRUMS) ? PositionMath::getDrumLaneCoordinates(gemColumn, endPosition, width, height) : PositionMath::getGuitarLaneCoordinates(gemColumn, endPosition, width, height);
    
    // Calculate lane widths based on sustain width parameter
    float startWidth = (startLane.rightX - startLane.leftX) * sustainWidth;
    float endWidth = (endLane.rightX - endLane.leftX) * sustainWidth;
    float radius = std::min(startWidth, endWidth) * SUSTAIN_CAP_RADIUS_SCALE;
    
    // Create paths for trapezoid and rounded caps
    auto trapezoid = columnRenderer.createTrapezoidPath(startLane, endLane, startWidth, endWidth);
    auto startCap = columnRenderer.createRoundedCapPath(startLane, startWidth, radius);

    // Scale end cap height proportionally to width for natural perspective
    float endCapHeightScale = endWidth / startWidth;
    auto endCap = columnRenderer.createRoundedCapPath(endLane, endWidth, radius, endCapHeightScale);

    // Render to offscreen image for perfect compositing
    auto sustainImage = columnRenderer.createOffscreenColumnImage(trapezoid, startCap, endCap, colour);
    
    // Draw final result
    g.setOpacity(opacity);
    auto bounds = trapezoid.getBounds().getUnion(startCap.getBounds()).getUnion(endCap.getBounds());
    g.drawImageAt(sustainImage, (int)bounds.getX() - 1, (int)bounds.getY() - 1);
}

