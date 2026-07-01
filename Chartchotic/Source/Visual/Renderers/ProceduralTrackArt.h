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

    // One strikeline pad as a screen-space quad (follows the neck curve): near
    // (toward player) and far corners, plus the lane's base colour.
    struct StrikePad
    {
        juce::Point<float> nearL, nearR, farL, farR;
        // Lane separator boundary to this pad's right (pre-inset, = the gridline
        // position). The connector groove is centred here so it lines up with the
        // lane-line, instead of drifting to the gap midpoint when neighbouring pads
        // differ in width. Unused for the last pad.
        juce::Point<float> sepNear, sepFar;
        juce::Colour baseColour;     // top of the vertical gradient (duller)
        juce::Colour bottomColour;   // bottom of the vertical gradient (brighter)
    };

    // Procedural strikeline: a dark strike-zone frame hugging the pad row + per-lane
    // pads (gradient colour border, darkened interior, split) + chrome connectors
    // (double bar between colours, single-bar end caps). Pixel-matched to the former
    // strikeline PNGs and used for every part (drums, guitar, elite), which lets it
    // follow any board width / lane count the fixed PNGs could not.
    // endCaps: draw a silver end bar just outside the first/last pad and extend the
    // dark frame to house it (drums/elite). When false the outer pads run full width
    // with only a thin dark margin and no cap (guitar-like references).
    // barHalfFrac: half-width of each silver separator bar as a fraction of the gap
    // between pads. Guitar's narrower lanes need a larger value to keep the chrome the
    // same visual width as the wider-lane references.
    void drawStrikelinePads(juce::Graphics& g,
                            const std::vector<StrikePad>& pads,
                            bool endCaps = true,
                            float barHalfFrac = 0.28f);
}
