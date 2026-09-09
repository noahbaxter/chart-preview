/*
    ==============================================================================

        TrackRenderer.h
        Author:  Noah Baxter

        Renders the track as composited layers: dark fretboard fill, sidebars,
        lane lines, strikeline, and instrument-specific elements (kick smashers
        or strikeline connectors). Layers are individually positionable via
        debug tuning parameters.

    ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../../Utils/ChartTypes.h"
#include "TrackFade.h"
#include "../Geometry/PositionMath.h"
#include "../Utils/DrawingConstants.h"

class TrackRenderer
{
public:
    TrackRenderer(juce::ValueTree& state);

    Part activePart = Part::GUITAR;

    void paint(juce::Graphics& g, int viewportWidth, int viewportHeight);

    /** Paint the scrolling highway texture overlay. Call between track and scene rendering. */
    void paintTexture(juce::Graphics& g, float scrollOffset, int targetWidth, int targetHeight);

    /** Paint Bemani mode overlays (lane dividers, strikeline). Call AFTER paintTexture. */
    void paintBemaniOverlay(juce::Graphics& g, int viewportWidth, int viewportHeight);

    /** Paint Bemani sidebar masks. Call AFTER scene renderer to clip sustain/note overflow. */
    void paintBemaniSidebars(juce::Graphics& g, int viewportWidth, int viewportHeight);

    /** Paint Bemani sidebar rails (decorative). Intended for DrawCallMap injection at TRACK_SIDEBARS. */
    void paintBemaniRails(juce::Graphics& g, int viewportWidth, int viewportHeight);

    /** Rebuild the cached faded track image. Call on resize, instrument change, or highway length change.
        overflow = extra pixels above the viewport (for extended VP Y). Bitmaps are (width, height+overflow).
        When geometryOnly=true, skip image baking (polygon fill + layer overlays) but still rebuild
        edge geometry cache and texture prebake. Used when a shared TrackImageCache provides images. */
    void rebuild(int width, int height, int overflow,
                 float farFadeEnd, float farFadeLen, float farFadeCurve,
                 float posEnd, bool geometryOnly = false);

    /** Invalidate cached state so the next rebuild() is not skipped. */
    void invalidate() { cached.width = -1; prebaked.valid = false; }

    /** Whether the cached geometry was built for drums (used to detect instrument change). */
    bool getCachedIsDrums() const { return cached.isDrums; }
    RenderType getCachedRenderType() const { return cached.renderType; }

    /** Set the source texture for highway overlay. */
    void setTexture(const juce::Image& texture);

    /** Clear the highway texture overlay. */
    void clearTexture();

    float textureScale = 1.0f;    // >1 = more tiles (squished), <1 = fewer (stretched)
    float textureOpacity = TEXTURE_OPACITY_DEFAULT;
    float tileStep = 0.80f;       // Vertical step between tiles (< 1.0 = overlap for baked fade)
    float tileScaleStep = 0.50f;   // Scale multiplier per tile (each tile = previous * this)

    // Per-layer positioning (debug-tunable)
    struct LayerTransform {
        float scale = 1.0f;
        float xOffset = 0.0f;
        float yOffset = 0.0f;
    };

    enum Layer { SIDEBARS = 0, LANE_LINES, STRIKELINE, CONNECTORS, NUM_LAYERS };
    LayerTransform layersGuitar[NUM_LAYERS] = {
        {0.940f, 0.0f,     0.0f},      // SIDEBARS
        {0.550f, 0.0f,     0.0f},      // LANE_LINES
        {0.750f, 0.0f, -0.1825f},      // STRIKELINE
        {0.835f, 0.0f, -0.1960f}       // CONNECTORS
    };
    LayerTransform layersDrums[NUM_LAYERS] = {
        {0.925f, 0.0f,     0.0f},      // SIDEBARS
        {0.440f, 0.0f,     0.0f},      // LANE_LINES
        {0.730f, 0.0f, -0.1815f},      // STRIKELINE
        {0.840f, 0.0f, -0.1600f}       // CONNECTORS
    };

    /** Set lane coords for perspective-projected lane line rendering. */
    void setLaneCoords(const PositionConstants::NormalizedCoordinates* coords, int count)
    {
        laneCoords_ = coords;
        laneCount_ = count;
    }

    /** Get a pre-baked layer image (with far-fade applied). Valid after rebuild(). */
    const juce::Image& getLayerImage(Layer layer) const { return layerImages[layer]; }

    /** Get the composited faded track base image. Valid after rebuild(). */
    const juce::Image& getFadedTrackImage() const { return fadedTrackImage; }

    /** Paint using an externally-provided cached faded track image (scales to viewport). */
    void paintFromCache(juce::Graphics& g, const juce::Image& cachedFadedTrack,
                        int viewportWidth, int viewportHeight)
    {
        if (PositionMath::bemaniMode) { paint(g, viewportWidth, viewportHeight); return; }
        if (!cachedFadedTrack.isValid()) return;
        if (cachedFadedTrack.getWidth() == viewportWidth && cachedFadedTrack.getHeight() == viewportHeight)
            g.drawImageAt(cachedFadedTrack, 0, 0);
        else
            g.drawImage(cachedFadedTrack, 0, 0, viewportWidth, viewportHeight,
                        0, 0, cachedFadedTrack.getWidth(), cachedFadedTrack.getHeight());
    }

private:
    const PositionConstants::NormalizedCoordinates* laneCoords_ = nullptr;
    int laneCount_ = 0;
    juce::ValueTree& state;

    // Layer source images
    juce::Image strikelineConnectorsImage;
    juce::Image kickSmashersImage;

    juce::Image fadedTrackImage;       // Dark fill + far-fade (base layer)
    juce::Image layerImages[NUM_LAYERS]; // Per-layer images with far-fade

    // Highway texture overlay
    juce::Image sourceTexture;
    juce::Image offscreen;
    bool textureEnabled = false;

    // Cached geometry (used by both track fade and texture scanline LUT)
    struct CachedGeometry {
        std::vector<std::pair<PositionConstants::LaneCorners, float>> edges;
        int stripCount = 0;
        int width = 0, height = 0;   // viewport dimensions
        int overflow = 0;            // extra pixels above viewport
        bool isDrums = false;
        RenderType renderType = RenderType::FIVE_FRET;   // distinguishes elite from 4-lane drums
        float posEnd = 0, fadeEnd = 0, fadeLen = 0, fadeCurve = 0;
        int totalHeight() const { return height + overflow; }
    } cached;

    // Pre-baked scanline data for fast per-frame texture rendering
    struct ScanlineInfo {
        float texV;    // base texture V coordinate (1.0 - position, before scroll/scale)
        float alpha;   // fade alpha at this scanline
        int leftX;     // fretboard left pixel bound
        int rightX;    // fretboard right pixel bound
    };

    struct MipLevel { int rowOffset; int width; };

    struct PrebakedData {
        std::vector<ScanlineInfo> scanlines;
        juce::Image mipAtlas;           // all mip levels stacked vertically in one image
        std::vector<MipLevel> mips;     // per-level: row offset into atlas + width
        int yMin = 0, yMax = 0;
        int tileHeight = 0;            // rows per mip level (same for all levels)
        bool valid = false;
    } prebaked;

    void rebuildPrebake();
    void compositeLayers(juce::Image& target, int w, int h, bool isDrums,
                         float posEnd, float farFadeEnd);
    void bakeLayerImage(juce::Image& out, const juce::Image& src, const LayerTransform& t,
                        int w, int h, int overflow, bool isDrums, bool tiled,
                        float farFadeEnd, float farFadeLen, float farFadeCurve,
                        float posEnd);

    void bakeLaneLinesPerspective(int w, int h, int overflow, bool isDrums,
                                   float farFadeEnd, float farFadeLen, float farFadeCurve,
                                   float posEnd);

    // Procedural side rails stroked along the board edges (cached.edges). Used
    // instead of the fixed sidebar PNG for render types whose board width/aspect
    // differ from the PNG's baked perspective (e.g. Elite drums' wider 8-lane board).
    void bakeSidebarRailsPerspective(int w, int h, int overflow, bool isDrums,
                                      float farFadeEnd, float farFadeLen, float farFadeCurve,
                                      float posEnd);

    // Procedural strikeline pads along the strike-plane lane geometry, baked into
    // the STRIKELINE layer instead of the fixed strikeline PNG (Elite / A/B flag).
    void bakeStrikelinePadsPerspective(int w, int h, int overflow, bool isDrums,
                                        float farFadeEnd, float farFadeLen, float farFadeCurve,
                                        float posEnd);

    static constexpr int PIXELS_PER_STRIP = 1;
    static constexpr int MIN_STRIPS = 40;
    static constexpr int MAX_STRIPS = 150;
    static constexpr int PREBAKE_QUALITY = 4;  // vertical oversampling for pre-baked tile
    static constexpr int MIN_MIP_WIDTH = 8;    // stop mip chain when width drops below this
    static constexpr float HIGHWAY_POS_START = PositionConstants::HIGHWAY_POS_START;
};
