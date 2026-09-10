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
#include <chrono>
#include <cstdio>
#include <vector>

#include "Utils/ChartTypes.h"
#include "UI/ControlConstants.h"
#include "Midi/Utils/TimeConverter.h"
#include "Visual/Managers/AssetManager.h"
#include "Visual/Renderers/SceneRenderer.h"
#include "Visual/Art/TrackRenderer.h"
#include "Visual/Geometry/PositionMath.h"
#include "Visual/Geometry/PositionConstants.h"
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
    // Everything is placed by integer beat index and positioned at beat * BEAT, and the
    // gridlines are generated from the same index, so notes land exactly on the grid.
    const double BEAT = 0.1;   // one gridline per beat; MEASURE every 4 beats
    auto put = [&](int beat, int col, Gem g, bool sp = false)
    {
        if (col >= 0 && col < (int)LANE_COUNT)
            s.track[beat * BEAT][col] = GemWrapper(g, sp);
    };
    // Cymbal lanes are 2,3,7,8; note lanes are 1,4,5,6. Map a lane + dynamic to its glyph.
    const bool cymLane[10] = { false,false,true,true,false,false,false,true,true,false };
    enum Dyn { GHOST, NORMAL, ACCENT };
    auto gemFor = [&](int lane, Dyn d) -> Gem {
        bool cym = cymLane[lane];
        if (d == GHOST)  return cym ? Gem::CYM_GHOST  : Gem::HOPO_GHOST;
        if (d == ACCENT) return cym ? Gem::CYM_ACCENT : Gem::TAP_ACCENT;
        return cym ? Gem::CYM : Gem::NOTE;
    };

    // Hi-Hat tuning surface at the FRONT (nearest = largest): closed and open hi-hats each at
    // ghost / normal / accent so the ghost-ring + accent-chevron overlay alignment is easy to
    // eyeball against the (taller than a standard cone) hi-hat art. Indifferent (ordinary yellow
    // cymbal) as a control. Marches up col 2: closed g/n/a, then open g/n/a, then indifferent.
    auto putHat = [&](int b, Gem g, HiHatState hh) { s.track[b * BEAT][2] = GemWrapper(g, false, hh); };
    putHat(1,  Gem::CYM_GHOST,  HiHatState::Open);
    putHat(3,  Gem::CYM,        HiHatState::Open);
    putHat(5,  Gem::CYM_ACCENT, HiHatState::Open);
    putHat(7,  Gem::CYM_GHOST,  HiHatState::Closed);
    putHat(9,  Gem::CYM,        HiHatState::Closed);
    putHat(11, Gem::CYM_ACCENT, HiHatState::Closed);
    putHat(13, Gem::CYM,        HiHatState::Indifferent);

    // Flams, up front where the two squished copies are big enough to judge. A drum lane
    // (snare) and a cymbal lane (ride), each flam / plain / ghost flam / accent flam, so the
    // pair can be compared straight across against its plain twin. Beat 16 is an Open Ghost
    // Flam: flam, dynamic and hat state all at once, which the spec calls a legal combination.
    auto putFlam = [&](int b, int col, Dyn d, HiHatState hh = HiHatState::None) {
        s.track[b * BEAT][col] = GemWrapper(gemFor(col, d), false, hh, /*flam=*/true);
    };
    // Odd beats only: the kick dynamics below sit on 2/4/6 and their full-width bars draw
    // over a hand gem sharing the beat.
    for (int lane : { 1, 7 })
    {
        putFlam(1, lane, NORMAL);
        put(3, lane, gemFor(lane, NORMAL));   // plain control
        putFlam(5, lane, GHOST);
        putFlam(7, lane, ACCENT);
    }
    putFlam(16, 2, GHOST, HiHatState::Open);

    // Kick dynamics up front, where they are big enough to eyeball: ghost / normal / accent.
    // The permutation block below also charts kicks, but it sits past the visible window.
    s.track[2 * BEAT][0]  = GemWrapper(Gem::HOPO_GHOST);
    s.track[4 * BEAT][0]  = GemWrapper(Gem::NOTE);
    s.track[6 * BEAT][0]  = GemWrapper(Gem::TAP_ACCENT);

    // Kick flam: both kicks on one tick, which is how the format notates it. They should
    // draw as the two halves of one bar, 2x left and 1x right, not two stacked full bars.
    s.track[8 * BEAT][0]                      = GemWrapper(Gem::NOTE);
    s.track[8 * BEAT][ELITE_KICK_2X_COLUMN]   = GemWrapper(Gem::NOTE);

    // Stomp and Splash pedal bars: mini kick-bars centered on the hi-hat. Charted twice each,
    // once sharing a kick beat above (the worst case for telling pedal from kick) and once on
    // an empty beat, so both reads are in one frame.
    s.track[4 * BEAT][ELITE_SPLASH_COLUMN]  = GemWrapper(Gem::SPLASH);
    s.track[6 * BEAT][ELITE_STOMP_COLUMN]   = GemWrapper(Gem::STOMP);
    s.track[10 * BEAT][ELITE_STOMP_COLUMN]  = GemWrapper(Gem::STOMP);
    s.track[12 * BEAT][ELITE_SPLASH_COLUMN] = GemWrapper(Gem::SPLASH);

    // Hi-hat ringing zones, charted as the resolver would emit them for the hats above: beat 3
    // cut clean by beat 5 (no tail), beat 5 and the beat 12 splash on the default 1/4 + 1/8 fade.
    s.sustains.push_back({ 3 * BEAT,  5 * BEAT,    (uint)ELITE_HIHAT_COLUMN,
                           SustainType::HIHAT, GemWrapper(Gem::NOTE),   5 * BEAT });
    s.sustains.push_back({ 5 * BEAT,  6.5 * BEAT,  (uint)ELITE_HIHAT_COLUMN,
                           SustainType::HIHAT, GemWrapper(Gem::NOTE),   6 * BEAT });
    s.sustains.push_back({ 12 * BEAT, 13.5 * BEAT, (uint)ELITE_HIHAT_COLUMN,
                           SustainType::HIHAT, GemWrapper(Gem::SPLASH), 13 * BEAT });

    // Permutation matrix, one chord per row so every combination is easy to compare:
    //   row = one dynamic across ALL 8 hand lanes + a kick;
    //   rows go GHOST -> NORMAL -> ACCENT, then the same three again in STAR POWER (white).
    // Read across a row = every lane at that dynamic; read down a lane = ghost/normal/accent.
    const Dyn dyns[3] = { GHOST, NORMAL, ACCENT };
    int beat = 14;
    for (bool sp : { false, true })
    {
        for (Dyn d : dyns)
        {
            for (int lane = 1; lane <= 8; ++lane)
                put(beat, lane, gemFor(lane, d), sp);
            // Elite kicks carry dynamics, so the kick takes the row's dynamic like the hand
            // lanes do (ghost = fainter bar, accent = thicker bar with a centre line).
            put(beat, 0, gemFor(0, d), sp);
            if (sp) put(beat, 9, gemFor(0, d), sp);   // 2x kick on the star-power rows
            beat += 2;                              // one empty beat between rows
        }
        beat += 1;                                  // extra gap between the normal and SP groups
    }

    // Then the lanes: roll/tremolo LANES on a few separated columns (snare, tom1, ride) so
    // each lane's column bounds are clear, plus the far-apart Tom 3 to exercise col 6.
    const int rollStart = beat + 2, rollEnd = rollStart + 4;
    for (int lane = 1; lane <= 8; ++lane)
        s.sustains.push_back({ rollStart * BEAT, rollEnd * BEAT, (uint)lane,
                               SustainType::LANE, GemWrapper(gemFor(lane, NORMAL)) });

    // Gridlines from the same beat index -> notes sit exactly on them.
    for (int b = 0; b * BEAT <= farEnd; ++b)
        s.gridlines.push_back({ b * BEAT, (b % 4 == 0) ? Gridline::MEASURE : Gridline::BEAT });

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
    // >0: repaint this many times and print per-phase timings instead of comparing PNGs.
    // The plugin's own benchmark target can't build (its asset list predates the repo
    // reorganisation), and this path already links the real renderers with real assets.
    int    benchIters = 0;
    // Elite flam shape, the same two knobs as the debug panel's Flam section.
    PositionConstants::FlamTypeWidths flamWidths = PositionConstants::FLAM_TYPE_WIDTHS;
    float  flamSpread = PositionConstants::FLAM_SUBLANE_SPREAD;
    bool   flamOneOverlay = PositionConstants::FLAM_SINGLE_OVERLAY;
    float  flamTilt = PositionConstants::FLAM_TILT;
    bool   railsOnly = false;    // alias for --only sidebars
    // Render only the named parts in isolation (empty = all). Parts:
    // board gridlines gems sidebars lanes strikeline connectors
    juce::StringArray onlyParts;
    juce::String dumpStomp;   // if set: save the baked Stomp bar asset alone and exit
    juce::String dumpKick;    // if set: save the baked kick bars (fat + elite thin) and exit
    bool bemani = false;      // --bemani: flat (Bemani) mode instead of perspective

    for (int i = 1; i < argc; ++i)
    {
        juce::String a(argv[i]);
        if      (a == "--dump-stomp" && i + 1 < argc) dumpStomp = argv[++i];
        else if (a == "--dump-kick"  && i + 1 < argc) dumpKick = argv[++i];
        else if (a == "--part"   && i + 1 < argc) partName = argv[++i];
        else if (a == "--width"  && i + 1 < argc) W = juce::String(argv[++i]).getIntValue();
        else if (a == "--aspect" && i + 1 < argc) aspect = juce::String(argv[++i]).getDoubleValue();
        else if (a == "--length" && i + 1 < argc) length = juce::String(argv[++i]).getFloatValue();
        else if (a == "--fret-width" && i + 1 < argc) fretWidth = juce::String(argv[++i]).getFloatValue();
        else if (a == "--scroll" && i + 1 < argc) scroll = juce::String(argv[++i]).getFloatValue();
        else if (a == "--bench" && i + 1 < argc) benchIters = juce::String(argv[++i]).getIntValue();
        else if (a == "--flam-width"  && i + 1 < argc) {
            float v = juce::String(argv[++i]).getFloatValue();   // sets every glyph type
            flamWidths = { v, v, v, v, v, v };
        }
        else if (a == "--flam-spread" && i + 1 < argc) flamSpread = juce::String(argv[++i]).getFloatValue();
        else if (a == "--flam-tilt" && i + 1 < argc) flamTilt = juce::String(argv[++i]).getFloatValue();
        else if (a == "--flam-two-overlays") flamOneOverlay = false;
        else if (a == "--rails-only") railsOnly = true;
        else if (a == "--bemani") bemani = true;
        else if (a == "--only" && i + 1 < argc) onlyParts.addTokens(argv[++i], ",", "");
        else if (! a.startsWith("--")) outPath = a;
    }
    PositionMath::bemaniMode = bemani;
    PositionMath::bemaniHwyScale = bemani ? length : 1.0f;
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

    // Isolation dump: save just the baked Stomp bar asset (for pixel-accurate iteration
    // against the source art), then exit — no scene, no highway.
    if (dumpKick.isNotEmpty())
    {
        // Fat over thin on one canvas: the two arcs must trace the same curve.
        auto* fat    = assets.getBarKickImage();
        auto* thin   = assets.getBarKickEliteImage();
        auto* accent = assets.getBarKickAccentImage();
        juce::Image sheet(juce::Image::ARGB, fat->getWidth(), fat->getHeight() * 3, true);
        juce::Graphics g(sheet);
        g.drawImageAt(*fat, 0, 0);
        g.drawImageAt(*thin, 0, fat->getHeight());
        g.drawImageAt(*accent, 0, fat->getHeight() * 2);
        juce::File f(dumpKick);
        f.deleteFile();
        juce::FileOutputStream os(f);
        juce::PNGImageFormat png;
        png.writeImageToStream(sheet, os);
        return 0;
    }

    if (dumpStomp.isNotEmpty())
    {
        juce::File f(dumpStomp);
        f.deleteFile();
        juce::FileOutputStream os(f);
        juce::PNGImageFormat png;
        png.writeImageToStream(*assets.getBarStompImage(), os);
        return 0;
    }

    SceneRenderer scene(state, assets);
    TrackRenderer track(state);
    scene.activePart = part;
    track.activePart = part;

    const bool isDrums = isDrumLike(part);
    const bool isElite = (part == Part::ELITE_DRUMS);
    if (aspect < 0.1) aspect = 4.0 / 3.0;   // same slant/height for every instrument

    // Every part renders into the SAME canvas (width W, aspect 4:3). Elite's extra width now
    // lives in its wider board COORDS (ELITE_BOARD_WIDTH_SCALE scales the fretboard + lanes),
    // not in a wider canvas -- so this matches the plugin, where a solo highway fills its slot
    // and the board being a larger fraction of it is what makes elite wider.
    const int renderHeight = std::max(1, juce::roundToInt((double)W / aspect));
    const int renderWidth  = W;
    juce::ignoreUnused(fretWidth);

    // Highway length: scale farFadeEnd (default 1.20) by the length multiplier so
    // the board renders a longer runway. highwayPosEnd (board geometry end) must
    // cover it. Set before overflow/rebuild so all baking uses the extended board.
    scene.farFadeEnd    = FAR_FADE_DEFAULT * length;
    scene.highwayPosEnd = std::max(PositionConstants::HIGHWAY_POS_END, scene.farFadeEnd);
    scene.flamTypeWidths    = flamWidths;
    scene.flamSubLaneSpread = flamSpread;
    scene.flamSingleOverlay = flamOneOverlay;
    scene.flamTilt          = flamTilt;

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
        getRenderType(part), scene.farFadeEnd, (uint)renderWidth, (uint)renderHeight,
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

    if (benchIters > 0)
    {
        scene.collectPhaseTiming = true;
        struct Acc { double notes = 0, sustains = 0, grid = 0, anim = 0, exec = 0, total = 0, board = 0; } acc;
        std::vector<double> totals;
        totals.reserve((size_t)benchIters);

        // Warmup has to cover every first-frame bake (curved-gem cache, flare tints, texture
        // prebake) or they land in the mean and swamp the steady-state cost.
        constexpr int kWarmup = 15;
        // One canvas reused across iterations: a fresh 1440x1462 ARGB is ~8 MB to allocate
        // and zero, which is several times the frame cost being measured.
        juce::Image benchCanvas(juce::Image::ARGB, renderWidth, totalH, true);
        for (int i = 0; i < benchIters + kWarmup; i++)
        {
            benchCanvas.clear(benchCanvas.getBounds());
            juce::Graphics g(benchCanvas);
            auto t0 = std::chrono::high_resolution_clock::now();
            if (showBoard)
            {
                track.paint(g, renderWidth, totalH);
                track.paintTexture(g, scroll, renderWidth, totalH);
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            if (overflow > 0)
                g.addTransform(juce::AffineTransform::translation(0.0f, (float)overflow));
            scene.paint(g, renderWidth, renderHeight, trackWindow, sustainWindow, gridlines,
                        flipRegions, eventMarkers, windowStart, windowEnd, false);
            if (i < kWarmup) continue;

            const auto& pt = scene.lastPhaseTiming;
            double boardUs = std::chrono::duration<double, std::micro>(t1 - t0).count();
            acc.notes += pt.notes_us;     acc.sustains += pt.sustains_us;
            acc.grid  += pt.gridlines_us; acc.anim     += pt.animation_us;
            acc.exec  += pt.execute_us;   acc.board    += boardUs;
            acc.total += pt.total_us + boardUs;
            totals.push_back(pt.total_us + boardUs);
        }

        std::sort(totals.begin(), totals.end());
        double n = (double)benchIters;
        std::printf("%-6s %dx%d  %d iters\n", partName.toRawUTF8(), renderWidth, totalH, benchIters);
        std::printf("  board(track+texture) %8.0f us\n", acc.board / n);
        std::printf("  notes                %8.0f us\n", acc.notes / n);
        std::printf("  sustains             %8.0f us\n", acc.sustains / n);
        std::printf("  gridlines            %8.0f us\n", acc.grid / n);
        std::printf("  animation            %8.0f us\n", acc.anim / n);
        std::printf("  execute              %8.0f us  (%d draw calls, %.1f us each)\n",
                    acc.exec / n, scene.lastPhaseTiming.drawCalls,
                    scene.lastPhaseTiming.drawCalls > 0
                        ? (acc.exec / n) / scene.lastPhaseTiming.drawCalls : 0.0);
        std::printf("  TOTAL mean           %8.0f us   p95 %.0f   (%.0f%% of a 60fps frame)\n",
                    acc.total / n, totals[(size_t)(totals.size() * 0.95)],
                    (acc.total / n) / 16666.7 * 100.0);
        return 0;
    }

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
