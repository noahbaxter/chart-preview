/*
    ==============================================================================

        PositionConstants.h
        Created by Claude Code (refactoring positioning logic)
        Author: Noah Baxter

        This file contains positioning data and lane coordinate calculations.
        All glyph positioning logic has been moved to GlyphRenderer.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

// Windows compatibility
#if defined(_WIN32) || defined(_WIN64) || defined(__WINDOWS__) || defined(_MSC_VER)
    typedef unsigned int uint;
#endif

class PositionConstants
{
public:
    // Lane coordinate system for sustain rendering
    struct LaneCorners
    {
        float leftX, rightX, centerY;
    };

    // Normalized coordinate data for positioning elements
    struct NormalizedCoordinates
    {
        float normY1, normY2;      // Y position at far (1) and near (0) end
        float normX1, normX2;      // X position at far and near end
        float normWidth1, normWidth2;  // Width at far and near end
    };

    // 3D perspective calculation parameters
    struct PerspectiveParams
    {
        float highwayDepth;
        float playerDistance;
        float perspectiveStrength;
        float exponentialCurve;
        float xOffsetMultiplier;
        float barNoteHeightRatio;
        float regularNoteHeightRatio;
    };

    PositionConstants() = default;
    ~PositionConstants() = default;

    //==============================================================================
    // Guitar Lane Coordinates (for column rendering)
    LaneCorners getGuitarLaneCoordinates(uint gemColumn, float position, uint width, uint height);

    //==============================================================================
    // Drum Lane Coordinates (for column rendering)
    LaneCorners getDrumLaneCoordinates(uint gemColumn, float position, uint width, uint height);

    //==============================================================================
    // Public access for GlyphRenderer
    NormalizedCoordinates getGuitarOpenNoteCoords();
    NormalizedCoordinates getGuitarNoteCoords(uint gemColumn);
    NormalizedCoordinates getDrumKickCoords();
    NormalizedCoordinates getDrumPadCoords(uint gemColumn);
    PerspectiveParams getPerspectiveParams() const;

private:
    //==============================================================================
    // Core perspective calculation
    juce::Rectangle<float> createPerspectiveGlyphRect(
        float position,
        float normY1, float normY2,
        float normX1, float normX2,
        float normWidth1, float normWidth2,
        bool isBarNote,
        uint width, uint height
    );

    //==============================================================================
    // Positioning Data: Guitar
    NormalizedCoordinates getGuitarLaneOpenCoords();
    NormalizedCoordinates getGuitarLaneNoteCoords(uint gemColumn);

    //==============================================================================
    // Positioning Data: Drums
    NormalizedCoordinates getDrumLaneKickCoords();
    NormalizedCoordinates getDrumLanePadCoords(uint gemColumn);
};
