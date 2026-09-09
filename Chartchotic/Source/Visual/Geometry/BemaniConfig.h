/*
    ==============================================================================

        BemaniConfig.h
        Author: Noah Baxter

        Centralized Bemani mode configuration. All tunable values live in a
        single struct with a global instance. The descriptor array drives the
        debug tuning panel.

    ==============================================================================
*/

#pragma once

struct BemaniConfig
{
    // Position group
    float strikelinePos      = 0.90f;
    float gemNudgeGuitar     = -0.010f;
    float gemNudgeDrums      = -0.010f;
    float cymNudge           = -0.010f;   // Drums only — cymbals tuned separately from toms (mirrors perspective cymZ)
    float barNudge           = 0.015f;
    float barFit             = 1.00f;
    float curvature          = 0.0f;
    float gemW               = 0.75f;
    float gemH               = 0.90f;
    float barW               = 1.12f;
    float barH               = 0.80f;

    // Sustains group
    float sustainWidth       = 0.20f;
    float barSustainWidth    = 1.00f;
    float sustainCap         = 0.21f;
    float sustStartOff       = -0.020f;
    float sustEndOff         = -0.025f;
    float laneStartOff       = -0.010f;
    float laneEndOff         = -0.020f;
    float laneStartPx        = 65.0f;
    float laneEndPxGuitar    = 20.0f;
    float laneEndPxDrums     = 20.0f;
    float barLaneStartPx     = 80.0f;
    float barLaneEndPxGuitar = 0.0f;
    float barLaneEndPxDrums  = 20.0f;
    float laneFillW          = 0.94f;
    float barLaneFillW       = 1.00f;
    float laneCap            = 0.10f;
    float barCap             = 0.03f;
    float laneDivW           = 1.0f;
    float barLaneW           = 1.0f;

    // Visual group
    float texSpeed           = 1.000f;
    float gridlineBoost      = 3.0f;
    float laneOpacity        = 0.20f;
    float strikelineOpacity  = 0.8f;
    float railInset          = -0.006f;
    float laneZ              = 0.0f;
    float sustainZ           = 0.0f;
    float gridlineZ          = 0.0f;
    float hitBarNudgeGuitar  = 0.0f;    // Bar hit Y nudge guitar (fraction of hit height)
    float hitBarNudgeDrums   = 0.10f;   // Bar hit Y nudge drums

    float hitBarNudge(bool isDrums) const { return isDrums ? hitBarNudgeDrums : hitBarNudgeGuitar; }

    // Highway scale (used in perspective mode too)
    float hwyScaleGuitar     = 1.00f;
    float hwyScaleDrums      = 1.00f;

    // Non-tunable (no slider)
    float noteYOffset        = -0.015f;

    // Per-instrument accessors
    float gemNudge(bool isDrums) const { return isDrums ? gemNudgeDrums : gemNudgeGuitar; }
    float laneEndPx(bool isDrums) const { return isDrums ? laneEndPxDrums : laneEndPxGuitar; }
    float barLaneEndPx(bool isDrums) const { return isDrums ? barLaneEndPxDrums : barLaneEndPxGuitar; }
    float hwyScale(bool isDrums) const { return isDrums ? hwyScaleDrums : hwyScaleGuitar; }
};

inline BemaniConfig bemaniConfig;

//==============================================================================
// Descriptor for the debug tuning panel

struct BemaniTunable
{
    const char* name;
    const char* group;
    float BemaniConfig::* field;
    float min, max, step;
    int decimals;
    bool featured = false;
};

inline constexpr BemaniTunable bemaniTunables[] = {
    // Position group
    {"Strike Y",       "Position", &BemaniConfig::strikelinePos,      0.70f, 0.99f, 0.01f,  2},
    {"Gem Nudge Gtr",  "Position", &BemaniConfig::gemNudgeGuitar,    -0.50f, 1.00f, 0.005f, 3},
    {"Gem Nudge Drm",  "Position", &BemaniConfig::gemNudgeDrums,    -0.50f, 1.00f, 0.005f, 3},
    {"Cym Nudge",      "Position", &BemaniConfig::cymNudge,         -0.50f, 1.00f, 0.005f, 3},
    {"Bar Nudge",      "Position", &BemaniConfig::barNudge,         -0.50f, 0.50f, 0.005f, 3},
    {"Bar Fit",        "Position", &BemaniConfig::barFit,             0.50f, 1.50f, 0.01f,  2},
    {"Curvature",      "Position", &BemaniConfig::curvature,          0.00f, 1.00f, 0.05f,  2},
    {"Gem W",          "Position", &BemaniConfig::gemW,               0.20f, 2.00f, 0.02f,  2},
    {"Gem H",          "Position", &BemaniConfig::gemH,               0.20f, 2.00f, 0.02f,  2},
    {"Bar W",          "Position", &BemaniConfig::barW,               0.20f, 2.00f, 0.02f,  2},
    {"Bar H",          "Position", &BemaniConfig::barH,               0.10f, 2.00f, 0.02f,  2},

    // Sustains group
    {"Sust W",         "Sustains", &BemaniConfig::sustainWidth,       0.10f, 1.50f, 0.05f,  2},
    {"Bar Sust W",     "Sustains", &BemaniConfig::barSustainWidth,    0.30f, 1.00f, 0.05f,  2},
    {"Sust Cap",       "Sustains", &BemaniConfig::sustainCap,         0.00f, 0.50f, 0.01f,  2},
    {"Sust Start",     "Sustains", &BemaniConfig::sustStartOff,      -0.10f, 0.05f, 0.005f, 3},
    {"Sust End",       "Sustains", &BemaniConfig::sustEndOff,        -0.05f, 0.10f, 0.005f, 3},
    {"Lane Start",     "Sustains", &BemaniConfig::laneStartOff,      -0.10f, 0.05f, 0.005f, 3},
    {"Lane End",       "Sustains", &BemaniConfig::laneEndOff,        -0.05f, 0.10f, 0.005f, 3},
    {"Lane Fill W",    "Sustains", &BemaniConfig::laneFillW,          0.20f, 1.50f, 0.02f,  2},
    {"Bar Fill W",     "Sustains", &BemaniConfig::barLaneFillW,       0.30f, 1.00f, 0.02f,  2},
    {"Lane Cap",       "Sustains", &BemaniConfig::laneCap,            0.00f, 0.50f, 0.01f,  2},
    {"Bar Cap",        "Sustains", &BemaniConfig::barCap,             0.00f, 0.20f, 0.005f, 3},
    {"Lane Div W",     "Sustains", &BemaniConfig::laneDivW,           0.20f, 3.00f, 0.05f,  2},
    {"Bar Lane W",     "Sustains", &BemaniConfig::barLaneW,           0.20f, 3.00f, 0.05f,  2},

    // Visual group
    {"Tex Speed",      "Visual",   &BemaniConfig::texSpeed,           0.50f, 3.00f, 0.005f, 3},
    {"Grid Boost",     "Visual",   &BemaniConfig::gridlineBoost,      0.50f, 5.00f, 0.10f,  1},
    {"Lane Alpha",     "Visual",   &BemaniConfig::laneOpacity,        0.00f, 1.00f, 0.02f,  2},
    {"Strike Alpha",   "Visual",   &BemaniConfig::strikelineOpacity,  0.00f, 1.00f, 0.02f,  2},
    {"Rail Inset",     "Visual",   &BemaniConfig::railInset,         -0.02f, 0.05f, 0.001f, 3},
    {"Lane Z",         "Visual",   &BemaniConfig::laneZ,            -20.0f, 20.0f,  0.5f,   1},
    {"Sustain Z",      "Visual",   &BemaniConfig::sustainZ,         -20.0f, 20.0f,  0.5f,   1},
    {"Grid Z",         "Visual",   &BemaniConfig::gridlineZ,        -20.0f, 20.0f,  0.5f,   1},
    {"HitBar Ndg Gtr", "Visual",   &BemaniConfig::hitBarNudgeGuitar, -0.50f, 0.50f, 0.01f, 2},
    {"HitBar Ndg Drm", "Visual",   &BemaniConfig::hitBarNudgeDrums,  -0.50f, 0.50f, 0.01f, 2},
    {"Lane Start Px",  "Visual",   &BemaniConfig::laneStartPx,     -40.0f,  80.0f,  1.0f,   0},
    {"Ln End Px Gtr",  "Visual",   &BemaniConfig::laneEndPxGuitar, -40.0f,  80.0f,  1.0f,   0},
    {"Ln End Px Drm",  "Visual",   &BemaniConfig::laneEndPxDrums,  -40.0f,  80.0f,  1.0f,   0},
    {"Bar Ln Start Px","Visual",   &BemaniConfig::barLaneStartPx,  -40.0f,  80.0f,  1.0f,   0},
    {"BrLnEnd Gtr",    "Visual",   &BemaniConfig::barLaneEndPxGuitar,-40.0f, 80.0f, 1.0f,   0},
    {"BrLnEnd Drm",    "Visual",   &BemaniConfig::barLaneEndPxDrums, -40.0f, 80.0f, 1.0f,   0},
};

inline constexpr int BEMANI_TUNABLE_COUNT = sizeof(bemaniTunables) / sizeof(bemaniTunables[0]);
