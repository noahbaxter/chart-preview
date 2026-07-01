/*
    ==============================================================================

        ProceduralTrackArt.h
        Author: Noah Baxter

        Pure procedural drawing of track frame elements (side rails, ...) from the
        perspective-projected edge geometry. Kept separate from TrackRenderer so the
        drawing logic is self-contained and reusable; TrackRenderer owns the image
        lifecycle and far-fade, and hands this a Graphics + the edge strips.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Utils/PositionConstants.h"

namespace ProceduralTrackArt
{
    // One perspective strip: fretboard edge (left/right/centerY) + its position.
    using EdgeStrip = std::pair<PositionConstants::LaneCorners, float>;

    // Two side rails matching sidebars.png cross-section (inner edge -> outer):
    // dark-grey inside edge | grey line | black track | thin grey line. railFrac =
    // total rail width as a fraction of the LOCAL board width, so the rail
    // foreshortens (thins) down the neck with the board.
    void drawSidebarRails(juce::Graphics& g,
                          const std::vector<EdgeStrip>& edges, int stripCount,
                          float railFrac = 0.044f);
}
