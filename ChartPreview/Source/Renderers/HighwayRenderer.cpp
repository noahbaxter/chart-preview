/*
  ==============================================================================

    HighwayRenderer.cpp
    Created: 15 Jun 2024 3:57:32pm
    Author:  Noah Baxter

  ==============================================================================
*/

#include "HighwayRenderer.h"
#include "DrawingConstants.h"
#include "HitAnimationManager.h"
#include "PositionConstants.h"

using namespace AnimationConstants;
using namespace PositionConstants;

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
    float sustainWidth = std::min(startRect.getWidth(), endRect.getWidth()) * SUSTAIN_WIDTH_MULTIPLIER;

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
    float top = endCenterY - sustainWidth * SUSTAIN_MARGIN_SCALE;
    float bottom = startCenterY + sustainWidth * SUSTAIN_MARGIN_SCALE;
    
    return juce::Rectangle<float>(left, top, right - left, bottom - top);
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

std::pair<juce::Rectangle<float>, juce::Rectangle<float>> HighwayRenderer::getSustainPositionRects(uint gemColumn, float startPosition, float endPosition)
{
    juce::Rectangle<float> startRect, endRect;

    if (isPart(state, Part::DRUMS)) {
        if (gemColumn >= 5) return std::make_pair(juce::Rectangle<float>(), juce::Rectangle<float>());
        startRect = glyphRenderer.getDrumGlyphRect(gemColumn, startPosition, width, height);
        endRect = glyphRenderer.getDrumGlyphRect(gemColumn, endPosition, width, height);
    } else {
        startRect = glyphRenderer.getGuitarGlyphRect(gemColumn, startPosition, width, height);
        endRect = glyphRenderer.getGuitarGlyphRect(gemColumn, endPosition, width, height);
    }

    return std::make_pair(startRect, endRect);
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
                if (gems[gemColumn].gem != Gem::NONE)
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
            // For guitar open notes, use the open animation frames; otherwise use kick frames
            juce::Image* animFrame = nullptr;
            if (isPart(state, Part::GUITAR) && anim.isOpen) {
                animFrame = assetManager.getOpenAnimationFrame(anim.currentFrame);
            } else {
                animFrame = assetManager.getKickAnimationFrame(anim.currentFrame);
            }

            if (animFrame)
            {
                juce::Rectangle<float> kickRect;
                if (isPart(state, Part::GUITAR)) {
                    kickRect = glyphRenderer.getGuitarGlyphRect(0, strikelinePosition, width, height);
                } else {
                    kickRect = glyphRenderer.getDrumGlyphRect(anim.is2xKick ? 6 : 0, strikelinePosition, width, height);
                }

                // Scale up the animation (wider and MUCH taller to match the bar note height)
                kickRect = kickRect.withSizeKeepingCentre(kickRect.getWidth() * KICK_ANIMATION_WIDTH_SCALE, kickRect.getHeight() * KICK_ANIMATION_HEIGHT_SCALE);

                g.setOpacity(1.0f);
                g.drawImage(*animFrame, kickRect);
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
                hitRect = glyphRenderer.getGuitarGlyphRect(anim.lane, strikelinePosition, width, height);
            } else {
                hitRect = glyphRenderer.getDrumGlyphRect(anim.lane, strikelinePosition, width, height);
            }

            // Scale up the animation (wider and much taller)
            hitRect = hitRect.withSizeKeepingCentre(hitRect.getWidth() * HIT_ANIMATION_WIDTH_SCALE, hitRect.getHeight() * HIT_ANIMATION_HEIGHT_SCALE);

            // Draw the flash frame
            if (hitFrame)
            {
                g.setOpacity(HIT_FLASH_OPACITY);
                g.drawImage(*hitFrame, hitRect);
            }

            // Draw the colored flare on top (with tint for the lane color)
            if (flareImage && anim.currentFrame <= HIT_FLARE_MAX_FRAME)
            {
                g.setOpacity(HIT_FLARE_OPACITY);
                g.drawImage(*flareImage, hitRect);
            }
        }
    }
}