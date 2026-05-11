#pragma once

#include <vector>
#include "../Visual/Utils/DrawingConstants.h"

//==============================================================================
// Control Ranges, Steps, and Defaults
// Single source of truth for all user-facing control parameters.
//==============================================================================

// Note Speed
constexpr int NOTE_SPEED_MIN         = 2;
constexpr int NOTE_SPEED_MAX         = 20;
constexpr int NOTE_SPEED_DEFAULT     = 7;
constexpr int NOTE_SPEED_BEMANI_MIN  = 0;   // Bemani display range: 0-30
constexpr int NOTE_SPEED_BEMANI_MAX  = 30;
constexpr int NOTE_SPEED_BEMANI_DEFAULT = 12;
// Conversion factor: bemani flat viewport shows ~1.7x more readable area than perspective
constexpr float NOTE_SPEED_BEMANI_RATIO = 12.0f / 7.0f;

// Gem Scale (continuous %)
constexpr int GEM_SCALE_MIN_PCT     = 20;
constexpr int GEM_SCALE_MAX_PCT     = 200;
constexpr int GEM_SCALE_STEP_PCT    = 5;
constexpr int GEM_SCALE_DEFAULT_PCT = 100;

// Bar Scale (continuous %) — kicks, opens, etc.
constexpr int BAR_SCALE_MIN_PCT     = 20;
constexpr int BAR_SCALE_MAX_PCT     = 200;
constexpr int BAR_SCALE_STEP_PCT    = 5;
constexpr int BAR_SCALE_DEFAULT_PCT = 100;

// Texture Scale (continuous %)
constexpr int TEX_SCALE_MIN_PCT     = 25;
constexpr int TEX_SCALE_MAX_PCT     = 400;
constexpr int TEX_SCALE_STEP_PCT    = 25;
constexpr int TEX_SCALE_DEFAULT_PCT = 100;

// Texture Opacity
constexpr int TEX_OPACITY_MIN     = 0;
constexpr int TEX_OPACITY_MAX     = 100;
constexpr int TEX_OPACITY_STEP    = 5;
constexpr int TEX_OPACITY_DEFAULT = 100;

// Highway Length (UI percentages derived from FAR_FADE drawing constants)
constexpr int HWY_LENGTH_MIN_PCT     = (int)(FAR_FADE_MIN * 100);
constexpr int HWY_LENGTH_MAX_PCT     = (int)(FAR_FADE_MAX * 100);
constexpr int HWY_LENGTH_STEP_PCT    = 10;
constexpr int HWY_LENGTH_DEFAULT_PCT = (int)(FAR_FADE_DEFAULT * 100);

// Calibration / Sync Offset
constexpr int CALIBRATION_MIN_MS  = -200;
constexpr int CALIBRATION_MAX_MS  = 2000;
constexpr int CALIBRATION_STEP_MS = 20;
constexpr int CALIBRATION_DEFAULT = 0;

// Enum selectors (1-based, matching state storage)
// Framerate: 1=15FPS, 2=30FPS, 3=60FPS, 4=Native
constexpr int FRAMERATE_DEFAULT = 3; // 60 FPS

// Latency buffer: 1=0ms, 2=250ms, 3=500ms, 4=750ms, 5=1000ms, 6=1500ms
constexpr int LATENCY_DEFAULT = 3; // 500ms

// HOPO: "autoHopo" (bool) + "hopoThresh" (0-based index: 0=Tight/120, 1=Default/170, 2=Loose/240)
constexpr int HOPO_THRESHOLD_DEFAULT = 1; // "Default" (170 ticks)

// Toggle defaults (true = on)
constexpr bool DEFAULT_SHOW_GEMS        = true;
constexpr bool DEFAULT_SHOW_BARS        = true;
constexpr bool DEFAULT_SHOW_SUSTAINS    = true;
constexpr bool DEFAULT_SHOW_LANES       = true;
constexpr bool DEFAULT_SHOW_GRIDLINES   = true;
constexpr bool DEFAULT_SHOW_HIT_INDICATORS = true;
constexpr bool DEFAULT_SHOW_STAR_POWER  = true;
constexpr bool DEFAULT_SHOW_TRACK       = true;
constexpr bool DEFAULT_SHOW_STRIKELINE  = true;
constexpr bool DEFAULT_SHOW_LANE_SEPARATORS = true;
constexpr bool DEFAULT_SHOW_HIGHWAY     = true;
constexpr bool DEFAULT_SHOW_FPS         = false;

//==============================================================================
// Layout sizing
//
//  ┌─────────────────── toolbar (toolbarRatio * editorW) ────────────────────┐
//  │ Logo  [Instrument] [Diff]   ◂ Speed ▸   ◂ Length ▸   [View] [⚙]       │
//  ├──── sub-toolbar (write mode only, single row + optional labels) ───────┤
//  │  MODE               GRID                     MODIFIER                  │
//  │  [Draw|Edit][BAR]   [Snap][◂1/8▸][◂Off▸]     [None|HOPO|Strum|Tap]    │
//  ├────────────────────────────────────────────────────────────────────────-┤
//  │                         highway                                        │
//  ├──── footer (footerRatio * editorW) ───────────────────────────────────-┤
//  │  Click place | Shift+drag paint           1: song    v1.2.3-dev        │
//  └───────────────────────────────────────────────────────────────────────-─┘

// Main toolbar strip
constexpr float TOOLBAR_RATIO          = 0.06f;  // fraction of editor width
constexpr int   TOOLBAR_MAX_HEIGHT     = 100;    // cap (px)

// Sub-toolbar (write mode row below main strip)
constexpr bool  SUBTOOLBAR_SHOW_LABELS = true;   // tiny group headers above controls
constexpr float SUBTOOLBAR_HEIGHT_RATIO = SUBTOOLBAR_SHOW_LABELS ? 1.1f : 0.85f;

// Sub-toolbar control scaling
constexpr float SUBTOOLBAR_WIDTH_SCALE  = 0.85f; // shrink/grow all control widths
constexpr float SUBTOOLBAR_HEIGHT_SCALE = 0.80f; // shrink/grow control height (rest becomes padding)
constexpr float SUBTOOLBAR_PAD_TOP      = 0.0f;  // extra space above controls (fraction of control area)
constexpr float SUBTOOLBAR_PAD_BOTTOM   = 0.1f; // extra space below controls (fraction of control area)
constexpr float SUBTOOLBAR_MODE_W    = 5.0f;
constexpr float SUBTOOLBAR_BAR_W     = 2.5f;
constexpr float SUBTOOLBAR_SNAP_W    = 1.5f;
constexpr float SUBTOOLBAR_STEP_W    = 3.0f;
constexpr float SUBTOOLBAR_MOD_W     = 10.0f;
constexpr float SUBTOOLBAR_CYM_W     = 3.0f;
constexpr float SUBTOOLBAR_COL_GAP   = 2.0f;

// Footer
constexpr float FOOTER_RATIO       = 0.055f;
constexpr int   FOOTER_MAX_HEIGHT  = 48;
constexpr float HELP_KEY_PAD_X     = 1.2f;  // horizontal padding around key badges (× space width)
constexpr float HELP_KEY_PAD_Y     = 1.0f; // vertical padding inside key badges (× line height)
constexpr bool DEFAULT_AUTO_HOPO        = true;
constexpr bool DEFAULT_KICK_2X          = true;
constexpr bool DEFAULT_DYNAMICS         = true;
constexpr bool DEFAULT_CYMBALS          = true; // PRO drums
constexpr bool DEFAULT_DISCO_FLIP       = true;

//==============================================================================
// Menu Enums (1-based, matching state storage)

// Instrument types — organized by input family.
// Values are 1-based to match state storage.
enum class Part
{
    // 5-fret family (same MIDI pitch layout: 60-100 across 4 difficulties)
    GUITAR = 1,         // PART GUITAR
    GUITAR_COOP,        // PART GUITAR COOP
    RHYTHM,             // PART RHYTHM
    BASS,               // PART BASS
    KEYS,               // PART KEYS (5-lane keys, uses guitar pitch mapping)

    // 6-fret / Guitar Hero Live (3 black + 3 white frets, different pitch layout)
    GHL_GUITAR,         // PART GUITAR GHL (also covers PART GUITAR COOP GHL, PART RHYTHM GHL)
    GHL_BASS,           // PART BASS GHL

    // Drums
    DRUMS,              // PART DRUMS (4-lane, 5-lane, Pro — variant via DrumType)
    ELITE_DRUMS,        // PART ELITE_DRUMS (separate track, 8-lane, 2 octaves/difficulty)

    // Vocals (pitch-based, not fret-based — completely different rendering)
    VOCALS,             // PART VOCALS
    HARMONIES,          // PART HARM1, PART HARM2, PART HARM3

    // Pro instruments (dedicated pitch layouts, not yet parseable)
    PRO_GUITAR,         // PART REAL_GUITAR, PART REAL_GUITAR_22
    PRO_BASS,           // PART REAL_BASS, PART REAL_BASS_22
    PRO_KEYS,           // PART REAL_KEYS_X/H/M/E (separate track per difficulty)
};

// How a Part renders on the highway — determines interpreter, lane count, gem style
enum class RenderType
{
    FIVE_FRET,          // 5-fret guitar/bass/keys (6 lanes: open + 5 frets)
    SIX_FRET,           // 6-fret GHL (3 black + 3 white)
    FOUR_LANE_DRUMS,    // Standard/Pro drums (kick + 4 pads/cymbals)
    FIVE_LANE_DRUMS,    // GH-style drums (kick + 3 pads + 2 cymbals)
    ELITE_DRUMS,        // 8-lane e-kit (kick + snare + 3 toms + 3 cymbals + pedal)
    VOCALS,             // Pitch-based vocal track
    PRO_GUITAR,         // 17/22-fret pro guitar/bass
    PRO_KEYS,           // 25-key two-octave keyboard
};

inline RenderType getRenderType(Part p)
{
    switch (p) {
        case Part::GUITAR:
        case Part::GUITAR_COOP:
        case Part::RHYTHM:
        case Part::BASS:
        case Part::KEYS:         return RenderType::FIVE_FRET;
        case Part::GHL_GUITAR:
        case Part::GHL_BASS:     return RenderType::SIX_FRET;
        case Part::DRUMS:        return RenderType::FOUR_LANE_DRUMS;
        case Part::ELITE_DRUMS:  return RenderType::ELITE_DRUMS;
        case Part::VOCALS:
        case Part::HARMONIES:    return RenderType::VOCALS;
        case Part::PRO_GUITAR:
        case Part::PRO_BASS:     return RenderType::PRO_GUITAR;
        case Part::PRO_KEYS:     return RenderType::PRO_KEYS;
        default:                 return RenderType::FIVE_FRET;
    }
}

// Convenience — most code just needs "guitar-like or drum-like"
inline bool isGuitarLike(Part p) { return getRenderType(p) == RenderType::FIVE_FRET; }
inline bool isDrumLike(Part p)   { auto r = getRenderType(p); return r == RenderType::FOUR_LANE_DRUMS || r == RenderType::FIVE_LANE_DRUMS || r == RenderType::ELITE_DRUMS; }

// Can this Part be rendered as a scrolling highway? (Vocals, Pro Keys etc. need different rendering)
inline bool isHighwayRenderable(Part p) { return isGuitarLike(p) || isDrumLike(p); }

// Track names recognized by discovery, split by implementation status.
// Implemented parts have full rendering support; unimplemented are parsed but not renderable.
struct TrackNameEntry { const char* name; Part part; };

inline const std::vector<TrackNameEntry>& getImplementedTrackNames()
{
    static const std::vector<TrackNameEntry> names = {
        { "PART GUITAR",      Part::GUITAR },
        { "PART GUITAR COOP", Part::GUITAR_COOP },
        { "PART RHYTHM",      Part::RHYTHM },
        { "PART BASS",        Part::BASS },
        { "PART KEYS",        Part::KEYS },
        { "PART DRUMS",       Part::DRUMS },
    };
    return names;
}

inline const std::vector<TrackNameEntry>& getUnimplementedTrackNames()
{
    static const std::vector<TrackNameEntry> names = {
        { "PART GUITAR GHL",     Part::GHL_GUITAR },
        { "PART BASS GHL",       Part::GHL_BASS },
        { "PART REAL_GUITAR",    Part::PRO_GUITAR },
        { "PART REAL_GUITAR_22", Part::PRO_GUITAR },
        { "PART REAL_BASS",      Part::PRO_BASS },
        { "PART REAL_BASS_22",   Part::PRO_BASS },
    };
    return names;
}

// Sort order: Guitar, guitar alts (GHL, Pro), Bass, bass alts, Keys, Drums, Vocals
inline int getPartSortOrder(Part p)
{
    switch (p) {
        case Part::GUITAR:      return 0;
        case Part::RHYTHM:      return 1;
        case Part::GUITAR_COOP: return 2;
        case Part::GHL_GUITAR:  return 3;
        case Part::PRO_GUITAR:  return 4;
        case Part::BASS:        return 5;
        case Part::GHL_BASS:    return 6;
        case Part::PRO_BASS:    return 7;
        case Part::KEYS:        return 8;
        case Part::PRO_KEYS:    return 9;
        case Part::DRUMS:       return 10;
        case Part::ELITE_DRUMS: return 11;
        case Part::VOCALS:      return 12;
        case Part::HARMONIES:   return 13;
        default:                return 99;
    }
}

// Short display name for Part (used in toggle labels, badges)
inline juce::String getPartDisplayName(Part p)
{
    switch (p) {
        case Part::GUITAR:      return "Guitar";
        case Part::GUITAR_COOP: return "Co-op";
        case Part::RHYTHM:      return "Rhythm";
        case Part::BASS:        return "Bass";
        case Part::KEYS:        return "Keys";
        case Part::GHL_GUITAR:  return "GHL";
        case Part::GHL_BASS:    return "GHL Bass";
        case Part::DRUMS:       return "Drums";
        case Part::ELITE_DRUMS: return "E-Drums";
        case Part::VOCALS:      return "Vocals";
        case Part::HARMONIES:   return "Harmony";
        case Part::PRO_GUITAR:  return "Pro Gtr";
        case Part::PRO_BASS:    return "Pro Bass";
        case Part::PRO_KEYS:    return "Pro Keys";
        default:                return "Unknown";
    }
}

// Per-instrument icon (BinaryData pointer + size)
struct PartIconData {
    const char* data;
    int size;
};

inline PartIconData getPartIcon(Part p)
{
#if JUCE_TARGET_HAS_BINARY_DATA
    switch (p) {
        case Part::DRUMS:
        case Part::ELITE_DRUMS: return { BinaryData::icon_drums_png,  BinaryData::icon_drums_pngSize };
        case Part::BASS:
        case Part::GHL_BASS:
        case Part::PRO_BASS:    return { BinaryData::icon_bass_png,   BinaryData::icon_bass_pngSize };
        case Part::KEYS:
        case Part::PRO_KEYS:    return { BinaryData::icon_keys_png,   BinaryData::icon_keys_pngSize };
        default:                return { BinaryData::icon_guitar_png, BinaryData::icon_guitar_pngSize };
    }
#else
    juce::ignoreUnused(p);
    return { nullptr, 0 };
#endif
}

// Drum track type — variants within PART DRUMS, distinguished by heuristics / song.ini
enum class DrumType { NORMAL = 1, PRO, FIVE_LANE };
enum class SkillLevel { EASY = 1, MEDIUM, HARD, EXPERT };
