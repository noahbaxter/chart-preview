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
#include "../Geometry/PositionConstants.h"

namespace ProceduralTrackArt
{
    // One perspective strip: fretboard edge (left/right/centerY) + its position.
    using EdgeStrip = std::pair<PositionConstants::LaneCorners, float>;

    // Side-rail cross-section geometry, shared so other art (the strikeline) can align
    // to the rails without guessing. Band widths run inner->outer in relative units;
    // the whole rail is shifted inward by insetUnits from the raw board edge.
    namespace RailGeom
    {
        constexpr float bandUnits[4] = { 1.0f, 1.4f, 2.0f, 1.0f }; // dark-grey|grey|black|grey
        constexpr float totalUnits   = 1.0f + 1.4f + 2.0f + 1.0f;  // 5.4
        constexpr float insetUnits   = 1.3f;                        // rail shifted inward by this
        constexpr float defaultFrac  = 0.044f;                     // rail width / local board width

        // Rail inner edge expressed as a fraction of the board's *scaled* width (the
        // same space xAtFrac / getColumnPosition use, i.e. board width x FRETBOARD_SCALE).
        // Part-independent: the board width and edge position cancel out, so every part's
        // rail inner edge lands at this one fraction. Lets the strikeline snap its outer
        // pads to the rails algorithmically instead of per-part tuning.
        constexpr float innerEdgeFraction(float railFrac = defaultFrac)
        {
            return (railFrac * insetUnits / totalUnits
                    + (PositionConstants::FRETBOARD_SCALE - 1.0f) * 0.5f)
                   / PositionConstants::FRETBOARD_SCALE;
        }
    }

    // Two side rails matching sidebars.png cross-section (inner edge -> outer):
    // dark-grey inside edge | grey line | black track | thin grey line. railFrac =
    // total rail width as a fraction of the LOCAL board width, so the rail
    // foreshortens (thins) down the neck with the board.
    void drawSidebarRails(juce::Graphics& g,
                          const std::vector<EdgeStrip>& edges, int stripCount,
                          float railFrac = RailGeom::defaultFrac);

    // One strikeline pad as a screen-space quad (follows the neck curve): near
    // (toward player) and far corners, plus the lane's base colour.
    struct StrikePad
    {
        juce::Point<float> nearL, nearR, farL, farR;
        // Curved near (bottom) and far (top) edges, sampled left->right along the neck
        // arc, so wide pads bow with the board instead of drawing a flat chord between
        // the corners. Endpoints equal the corners above. Connectors/caps still use the
        // corners (they are narrow enough that the chord matches).
        std::vector<juce::Point<float>> nearEdge, farEdge;
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
