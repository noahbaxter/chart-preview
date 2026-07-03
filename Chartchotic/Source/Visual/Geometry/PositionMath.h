/*
    ==============================================================================

        PositionMath.h
        Created by Claude Code (refactoring positioning logic)
        Author: Noah Baxter

        This file contains mathematical functions for computing glyph positions
        and lane coordinates using 3D perspective calculations.
        All coordinate data is defined in PositionConstants.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PositionConstants.h"
#include "BemaniConfig.h"
#include "../../UI/ControlConstants.h"   // RenderType

// Windows compatibility
#if defined(_WIN32) || defined(_WIN64) || defined(__WINDOWS__) || defined(_MSC_VER)
    typedef unsigned int uint;
#endif

class PositionMath
{
public:
    PositionMath() = default;
    ~PositionMath() = default;

    static bool bemaniMode;
    static float bemaniHwyScale;  // farFadeEnd, controls vertical spread in flat mode

#ifdef DEBUG
    static PositionConstants::PerspectiveParams debugPerspParamsGuitar;
    static PositionConstants::PerspectiveParams debugPerspParamsDrums;
    static bool debugPolyShade;

    static const PositionConstants::PerspectiveParams& perspParams(bool isDrums)
    {
        return isDrums ? debugPerspParamsDrums : debugPerspParamsGuitar;
    }
#endif

    //==============================================================================
    // Bezier positioning system. Primary signatures take a RenderType so each
    // instrument (guitar / 4-lane drums / elite drums) gets its own fretboard
    // geometry (elite is wider). The bool overloads (guitar vs drums, no elite) are
    // kept for bemani/legacy call sites that never see elite.
    static PositionConstants::LaneCorners getFretboardEdge(
        RenderType renderType, float position, uint width, uint height,
        float posStart, float posEnd);

    static PositionConstants::LaneCorners getColumnPosition(
        RenderType renderType, float position, uint width, uint height,
        float posStart, float posEnd,
        const PositionConstants::NormalizedCoordinates& colCoords,
        float sizeScale, float fretboardScale = 1.0f,
        int bemaniLaneIdx = -1);

    static PositionConstants::LaneCorners getFretboardEdge(
        bool isDrums, float position, uint width, uint height,
        float posStart, float posEnd)
    {
        return getFretboardEdge(isDrums ? RenderType::FOUR_LANE_DRUMS : RenderType::FIVE_FRET,
                                position, width, height, posStart, posEnd);
    }

    static PositionConstants::LaneCorners getColumnPosition(
        bool isDrums, float position, uint width, uint height,
        float posStart, float posEnd,
        const PositionConstants::NormalizedCoordinates& colCoords,
        float sizeScale, float fretboardScale = 1.0f,
        int bemaniLaneIdx = -1)
    {
        return getColumnPosition(isDrums ? RenderType::FOUR_LANE_DRUMS : RenderType::FIVE_FRET,
                                 position, width, height, posStart, posEnd,
                                 colCoords, sizeScale, fretboardScale, bemaniLaneIdx);
    }

    // Distance of a column center from the fretboard center, normalized to half
    // the fretboard width. Range [-1, 1] — used by curvature math (arc = curv*(1-d²)).
    static float columnDistFromCenter(
        const PositionConstants::NormalizedCoordinates& fbCoords,
        const PositionConstants::NormalizedCoordinates& colCoords)
    {
        float fbCenter = fbCoords.normX1 + fbCoords.normWidth1 * 0.5f;
        float fbHalfW = fbCoords.normWidth1 * 0.5f;
        float colCenter = colCoords.normX1 + colCoords.normWidth1 * 0.5f;
        return (colCenter - fbCenter) / fbHalfW;
    }

    // Compute a Bemani bar rectangle centered on the fretboard at a given position.
    // sizeScale = BAR_SIZE, imageAspect = glyph width/height. Bemani is flat (no foreshorten).
    static juce::Rectangle<float> computeBemaniBarRect(
        bool isDrums, float position, uint width, uint height,
        float posEnd, float sizeScale, float imageAspect)
    {
        auto fbEdge = getFretboardEdge(isDrums, position, width, height,
                                        PositionConstants::HIGHWAY_POS_START, posEnd);
        float fbWidth = fbEdge.rightX - fbEdge.leftX;
        float barFit = bemaniConfig.barFit * bemaniConfig.barLaneW;
        float colWidth = fbWidth * barFit * sizeScale;
        float colHeight = colWidth / imageAspect;
        float cx = (fbEdge.leftX + fbEdge.rightX) * 0.5f;
        return juce::Rectangle<float>(cx - colWidth * 0.5f, fbEdge.centerY - colHeight * 0.5f, colWidth, colHeight);
    }

private:
    //==============================================================================
    // Core perspective calculation
    static juce::Rectangle<float> createPerspectiveGlyphRect(
        const PositionConstants::PerspectiveParams& perspParams,
        float position,
        float normY1, float normY2,
        float normX1, float normX2,
        float normWidth1, float normWidth2,
        bool isBarNote,
        uint width, uint height
    );

};
