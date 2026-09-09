/*
    ==============================================================================

        render_main.cpp

        Headless render-to-PNG harness. Constructs the highway renderer stack
        (SceneRenderer + TrackRenderer + AssetManager) with fake note data and
        writes a single PNG to disk. No plugin host, no REAPER, no OpenGL.

        Usage:
            render_harness <out.png> [--part drums|guitar] [--width W]
                           [--aspect A] [--scroll S]

        Replicates HighwayComponent's real geometry so the aspect-fixed layer
        art (strikeline / sidebars / connectors) lines up with the procedural
        fretboard: render size is width x (width / aspect) plus a computed
        topOverflow band above, and the paint order is
            track.paint -> track.paintTexture -> (+overflow translate) -> scene.paint

    ==============================================================================
*/

#include <JuceHeader.h>
#include <iostream>
#include <cmath>
#include <algorithm>

#include "Utils/ChartTypes.h"
#include "UI/ControlConstants.h"
#include "Midi/Utils/TimeConverter.h"
#include "Visual/Managers/AssetManager.h"
#include "Visual/Renderers/SceneRenderer.h"
#include "Visual/Renderers/TrackRenderer.h"
#include "Visual/Utils/PositionMath.h"
#include "Visual/Utils/PositionConstants.h"
#include "Visual/Utils/DrawingConstants.h"

// A comprehensive fake chart exercising every glyph type the renderer can draw,
// spread down the highway (windowSpan == 1, so position == time == perspective
// depth). Roll lanes + sustains sit toward the far end. Content runs out to
// ~depth 3.3 so it fills a 300%-length highway.
struct FakeScene
{
    TimeBasedTrackWindow   track;
    TimeBasedSustainWindow sustains;
    TimeBasedGridlineMap   gridlines;
};

static FakeScene makeComprehensiveScene(bool isDrums, float farEnd)
{
    FakeScene s;
    auto put = [&](double pos, int col, Gem g, bool sp = false)
    {
        auto& f = s.track[pos];
        if (col >= 0 && col < (int)LANE_COUNT)
            f[col] = GemWrapper(g, sp);
    };

    if (isDrums)
    {
        // Columns: 0 kick, 1 red(snare), 2 yellow, 3 blue, 4 green, 6 2x-kick.
        // Basic gems
        put(0.05, 0, Gem::NOTE);  put(0.12, 1, Gem::NOTE);  put(0.20, 2, Gem::CYM);
        put(0.28, 3, Gem::CYM);   put(0.36, 4, Gem::CYM);
        // Dynamics: ghost + accent, note + cymbal
        put(0.46, 1, Gem::HOPO_GHOST);  put(0.54, 1, Gem::TAP_ACCENT);
        put(0.62, 2, Gem::CYM_GHOST);   put(0.70, 3, Gem::CYM_ACCENT);
        // 2x kick, then a chord
        put(0.78, 6, Gem::NOTE);
        put(0.88, 0, Gem::NOTE);  put(0.88, 1, Gem::NOTE);  put(0.88, 4, Gem::CYM);
        // Star power section (white gems; state "starPower" enabled below)
        put(1.00, 1, Gem::NOTE, true);  put(1.10, 2, Gem::CYM, true);
        put(1.20, 0, Gem::NOTE, true);
        // Spread further back
        put(1.40, 1, Gem::NOTE);  put(1.55, 3, Gem::CYM);  put(1.70, 2, Gem::CYM);
        put(1.90, 4, Gem::CYM);   put(2.05, 1, Gem::TAP_ACCENT);
        // Roll lanes + kick lane toward the end
        s.sustains.push_back({2.30, 2.85, 1u, SustainType::LANE, GemWrapper(Gem::NOTE)});
        s.sustains.push_back({2.30, 2.85, 3u, SustainType::LANE, GemWrapper(Gem::CYM)});
        s.sustains.push_back({2.95, 3.30, 0u, SustainType::LANE, GemWrapper(Gem::NOTE)});
    }
    else
    {
        // Columns: 0 open(bar), 1 green, 2 red, 3 yellow, 4 blue, 5 orange.
        put(0.05, 0, Gem::NOTE);  // open bar
        put(0.12, 1, Gem::NOTE);  put(0.20, 2, Gem::NOTE);  put(0.28, 3, Gem::NOTE);
        put(0.36, 4, Gem::NOTE);  put(0.44, 5, Gem::NOTE);
        // HOPOs and taps
        put(0.54, 1, Gem::HOPO_GHOST);  put(0.62, 3, Gem::HOPO_GHOST);
        put(0.70, 2, Gem::TAP_ACCENT);  put(0.78, 4, Gem::TAP_ACCENT);
        // Chord
        put(0.88, 1, Gem::NOTE);  put(0.88, 3, Gem::NOTE);  put(0.88, 5, Gem::NOTE);
        // Sustains (fret + open)
        s.sustains.push_back({0.98, 1.35, 1u, SustainType::SUSTAIN, GemWrapper(Gem::NOTE)});
        s.sustains.push_back({0.98, 1.35, 4u, SustainType::SUSTAIN, GemWrapper(Gem::NOTE)});
        s.sustains.push_back({1.45, 1.85, 0u, SustainType::SUSTAIN, GemWrapper(Gem::NOTE)});
        // Star power section
        put(2.00, 2, Gem::NOTE, true);  put(2.15, 4, Gem::NOTE, true);
        // Lane toward the end
        s.sustains.push_back({2.40, 2.90, 3u, SustainType::LANE, GemWrapper(Gem::NOTE)});
    }

    // Gridlines: measure every 4th, beat otherwise, out to the far end.
    int idx = 0;
    for (double p = 0.0; p <= farEnd; p += 0.1, ++idx)
        s.gridlines.push_back({ p, (idx % 4 == 0) ? Gridline::MEASURE : Gridline::BEAT });

    return s;
}

// Elite: a fake ED chart exercising all 8 hand lanes plus kicks. Columns:
// 0 kick, 1 Snare, 2 Hi-Hat, 3 L-Crash, 4 Tom1, 5 Tom2, 6 Tom3, 7 Ride,
// 8 R-Crash, 9 2x-kick. Cymbal lanes (2,3,7,8) carry CYM gems; the rest NOTE.
static FakeScene makeEliteScene(float farEnd)
{
    FakeScene s;
    auto put = [&](double pos, int col, Gem g, bool sp = false)
    {
        auto& f = s.track[pos];
        if (col >= 0 && col < (int)LANE_COUNT)
            f[col] = GemWrapper(g, sp);
    };
    auto gemForLane = [](int lane) {
        return (lane == 2 || lane == 3 || lane == 7 || lane == 8) ? Gem::CYM : Gem::NOTE;
    };

    // Staircase: one gem per hand lane 1..8 marching down the neck.
    double p = 0.08;
    for (int lane = 1; lane <= 8; ++lane, p += 0.10)
        put(p, lane, gemForLane(lane));

    // Kicks every quarter across the runway.
    for (double k = 0.05; k <= farEnd; k += 0.25)
        put(k, 0, Gem::NOTE);

    // A 2x kick, then a full 8-lane chord further back.
    put(1.00, 9, Gem::NOTE);
    for (int lane = 1; lane <= 8; ++lane)
        put(1.20, lane, gemForLane(lane));

    // Star-power (white) row toward the far end.
    for (int lane = 1; lane <= 8; ++lane)
        put(1.60, lane, gemForLane(lane), true);

    int idx = 0;
    for (double gp = 0.0; gp <= farEnd; gp += 0.1, ++idx)
        s.gridlines.push_back({ gp, (idx % 4 == 0) ? Gridline::MEASURE : Gridline::BEAT });

    return s;
}

int main(int argc, char** argv)
{
    juce::String outPath;
    juce::String partName = "drums";
    int    W = 720;
    double aspect = 0.0;         // 0 = auto (1.5 for elite's wider board, 4:3 otherwise)
    float  scroll = 0.0f;
    float  length = 3.0f;        // highway length multiplier (3.0 = 300%); scales farFadeEnd
    float  fretWidth = PositionConstants::ELITE_BOARD_WIDTH_SCALE;   // elite box width multiplier (wider render box, same drum slant)
    bool   railsOnly = false;    // alias for --only sidebars
    // Render only the named parts in isolation (empty = all). Parts:
    // board gridlines gems sidebars lanes strikeline connectors
    juce::StringArray onlyParts;

    for (int i = 1; i < argc; ++i)
    {
        juce::String a(argv[i]);
        if      (a == "--part"   && i + 1 < argc) partName = argv[++i];
        else if (a == "--width"  && i + 1 < argc) W = juce::String(argv[++i]).getIntValue();
        else if (a == "--aspect" && i + 1 < argc) aspect = juce::String(argv[++i]).getDoubleValue();
        else if (a == "--length" && i + 1 < argc) length = juce::String(argv[++i]).getFloatValue();
        else if (a == "--fret-width" && i + 1 < argc) fretWidth = juce::String(argv[++i]).getFloatValue();
        else if (a == "--scroll" && i + 1 < argc) scroll = juce::String(argv[++i]).getFloatValue();
        else if (a == "--rails-only") railsOnly = true;
        else if (a == "--only" && i + 1 < argc) onlyParts.addTokens(argv[++i], ",", "");
        else if (! a.startsWith("--")) outPath = a;
    }
    if (outPath.isEmpty()) outPath = "highway.png";
    if (length < 0.1f) length = 1.0f;

    juce::ScopedJuceInitialiser_GUI juceInit;

    Part part = Part::DRUMS;
    if      (partName == "guitar") part = Part::GUITAR;
    else if (partName == "elite")  part = Part::ELITE_DRUMS;

    juce::ValueTree state("PluginState");
    state.setProperty("part",              (int)part,               nullptr);
    state.setProperty("skillLevel",        (int)SkillLevel::EXPERT, nullptr);
    state.setProperty("drumType",          (int)DrumType::PRO,      nullptr);
    state.setProperty("autoHopo",          false,                   nullptr);
    state.setProperty("hopoThreshold",     2,                       nullptr);
    state.setProperty("starPower",         1,                       nullptr);  // enable SP so white gems render
    state.setProperty("hitIndicators",     0,                       nullptr);
    state.setProperty("dynamics",          1,                       nullptr);
    state.setProperty("kick2x",            1,                       nullptr);
    state.setProperty("showTrack",         true,                    nullptr);
    state.setProperty("showLaneSeparators", true,                   nullptr);
    state.setProperty("showStrikeline",    true,                    nullptr);

    AssetManager assets;
    SceneRenderer scene(state, assets);
    TrackRenderer track(state);
    scene.activePart = part;
    track.activePart = part;

    const bool isDrums = isDrumLike(part);
    const bool isElite = (part == Part::ELITE_DRUMS);
    if (aspect < 0.1) aspect = 4.0 / 3.0;   // same slant/height for every instrument

    // All highways share the same perspective/slant (drum geometry) and the same
    // render HEIGHT. Elite just renders into a WIDER box (renderWidth) so its 8
    // lanes get more absolute horizontal room -- same look, wider, not scaled.
    // Because elite reuses the proven drum geometry (a fixed fraction of the box),
    // its bottom never clips (drums don't). fretWidth = elite box width multiplier.
    const int renderHeight = std::max(1, juce::roundToInt((double)W / aspect));
    const int renderWidth  = isElite ? std::max(1, juce::roundToInt((double)W * fretWidth)) : W;

    // Highway length: scale farFadeEnd (default 1.20) by the length multiplier so
    // the board renders a longer runway. highwayPosEnd (board geometry end) must
    // cover it. Set before overflow/rebuild so all baking uses the extended board.
    scene.farFadeEnd    = FAR_FADE_DEFAULT * length;
    scene.highwayPosEnd = std::max(PositionConstants::HIGHWAY_POS_END, scene.farFadeEnd);

    scene.rescaleAssets(renderWidth);
    if (isElite)
        track.setLaneCoords(PositionConstants::eliteDrumBezierLaneCoords.data(),
                            (int)PositionConstants::ELITE_DRUM_LANE_COUNT);
    else
        track.setLaneCoords(isDrums ? scene.drumLaneCoordsLocal : scene.guitarLaneCoordsLocal,
                            isDrums ? (int)PositionConstants::DRUM_LANE_COUNT
                                    : (int)PositionConstants::GUITAR_LANE_COUNT);

    // Replicate HighwayComponent::updateOverflow(): the far end of the highway
    // sits above the visible slot, in a computed overflow band.
    auto farEdge = PositionMath::getFretboardEdge(
        isDrums, scene.farFadeEnd, (uint)renderWidth, (uint)renderHeight,
        PositionConstants::HIGHWAY_POS_START, scene.highwayPosEnd);
    const int overflow = std::max(0, (int)std::ceil(-farEdge.centerY));
    const int totalH   = renderHeight + overflow;

    track.rebuild(renderWidth, renderHeight, overflow,
                  scene.farFadeEnd, scene.farFadeLen, scene.farFadeCurve,
                  scene.highwayPosEnd);

    // Part isolation: render only the named parts (empty list = everything).
    if (railsOnly) { onlyParts.clearQuick(); onlyParts.add("sidebars"); }
    auto has = [&](const char* p) { return onlyParts.isEmpty() || onlyParts.contains(p); };
    const bool showBoard      = has("board");
    const bool showGems       = has("gems");
    const bool showGridlines  = has("gridlines");
    const bool showSidebars   = has("sidebars");
    const bool showLanes      = has("lanes");
    const bool showStrikeline = has("strikeline");
    const bool showConnectors = has("connectors");

    // Wire baked track layer overlays into the scene (perspective mode).
    scene.overlayYOffset = overflow;
    scene.clearOverlays();
    if (showStrikeline) scene.setOverlay(DrawOrder::TRACK_STRIKELINE, &track.getLayerImage(TrackRenderer::STRIKELINE));
    if (showLanes)      scene.setOverlay(DrawOrder::TRACK_LANE_LINES, &track.getLayerImage(TrackRenderer::LANE_LINES));
    if (showSidebars)   scene.setOverlay(DrawOrder::TRACK_SIDEBARS,   &track.getLayerImage(TrackRenderer::SIDEBARS));
    if (showConnectors) scene.setOverlay(DrawOrder::TRACK_CONNECTORS, &track.getLayerImage(TrackRenderer::CONNECTORS));

    FakeScene fake = isElite ? makeEliteScene(scene.farFadeEnd)
                             : makeComprehensiveScene(isDrums, scene.farFadeEnd);
    TimeBasedTrackWindow   trackWindow   = showGems ? fake.track    : TimeBasedTrackWindow{};
    TimeBasedSustainWindow sustainWindow = showGems ? fake.sustains : TimeBasedSustainWindow{};
    TimeBasedGridlineMap   gridlines     = showGridlines ? fake.gridlines : TimeBasedGridlineMap{};
    TimeBasedFlipRegions   flipRegions;
    TimeBasedEventMarkers  eventMarkers;

    // windowSpan == 1 so note time == perspective depth; content is authored in
    // depth units directly, out past 1.0 into the extended board.
    const double windowStart = 0.0;
    const double windowEnd   = 1.0;

    juce::Image canvas(juce::Image::ARGB, renderWidth, totalH, true);
    {
        juce::Graphics g(canvas);
        if (showBoard)
        {
            track.paint(g, renderWidth, totalH);
            track.paintTexture(g, scroll, renderWidth, totalH);
        }
        if (overflow > 0)
            g.addTransform(juce::AffineTransform::translation(0.0f, (float)overflow));
        scene.paint(g, renderWidth, renderHeight, trackWindow, sustainWindow, gridlines, flipRegions, eventMarkers,
                    windowStart, windowEnd, false);
    }

    juce::File out = outPath.startsWithChar('/')
                       ? juce::File(outPath)
                       : juce::File::getCurrentWorkingDirectory().getChildFile(outPath);
    out.deleteFile();

    juce::FileOutputStream os(out);
    if (! os.openedOk())
    {
        std::cerr << "Failed to open output: " << out.getFullPathName() << "\n";
        return 1;
    }
    juce::PNGImageFormat png;
    png.writeImageToStream(canvas, os);
    os.flush();
    std::cout << "Wrote " << out.getFullPathName()
              << " (" << renderWidth << "x" << totalH << ", slot " << renderWidth << "x" << renderHeight
              << ", overflow " << overflow << ")\n";
    return 0;
}
