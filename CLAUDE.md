# Chartchotic

VST/AU plugin visualizing MIDI as rhythm game charts (Clone Hero/YARG style).

## JUCE Source

DO NOT reference `/Applications/JUCE`. JUCE is a git submodule at `third_party/JUCE/` with
REAPER modifications baked in. Check `docs/REAPER_INTEGRATION.md` for REAPER-specific details.

## Building

Local build (macOS): `./build.sh` (builds VST3+AU, installs)
Options: `release`, `clean`, `--reaper`, `--vst3-only`, `--au-only`, `--standalone`

Release builds are handled by CI. Push to `dev` → `dev-latest` prerelease. Merge to `main`
with VERSION change → tagged release. Version source of truth: `VERSION` file (injected via
CMake). macOS builds are signed + notarized. Windows builds include Inno Setup installer.

## Versioning

`VERSION` file at repo root is the single source of truth. CI injects it into `.jucer` before
Projucer runs. The `JucePlugin_VersionString` define is used in code (not hardcoded strings).
Build channel (`DEV`/`RELEASE`) is injected via preprocessor define `CHARTCHOTIC_BUILD_CHANNEL`.

## Watch Out

**Threading**: Audio thread processes MIDI, GUI thread renders. When updating `noteStateMapArray`,
hold `noteStateMapLock` for the ENTIRE clear+write operation. Never split it — the renderer reads
between calls and you'll get flicker/race conditions. (Burned us in v0.8.6.)

**Perspective math**: All perspective/positioning now flows through the bezier system in
`PositionMath` (via `getColumnPosition` → `getFretboardEdge` → `createPerspectiveGlyphRect`).
`GlyphRenderer` only handles overlay positioning (drum accent scale). Fretboard width scales
are tunable via debug sliders on `SceneRenderer` and passed through to `AnimationRenderer`.

**Chart format specs**: `.mid` note types, modifiers, MIDI mappings live at
`../_refs/midi/` (quick reference) and `../chart-formats/docs/Chart-File-Formats/mid-format/`
(full docs). Covers 5-fret guitar, drums, 6-fret, pro guitar, vocals.

**Roadmap & Bugs**: `BACKLOG.md` — check before starting work to avoid duplicating known issues.
