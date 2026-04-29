# Backlog

Work from the top.

---

## Up Next

- **Test coverage expansion** — See details below in Test Coverage section.
- **UI scale setting** — User-adjustable scale factor for toolbar, footer, and panel elements (not highway rendering). Lets users with high-DPI or small screens resize the chrome independently.
- **All-difficulty overlay mode** — Show all 4 difficulties squeezed into the same highway. Each lane gets up to 4 notes stacked when all difficulties hit them. Color-code by difficulty instead of track color. All note data except specific pitches is shared across difficulties in the MIDI spec, so the data model supports this naturally.
- **Save/load presets + default state** — Persist user settings to disk. "Save as default" makes all new plugin instances start with custom settings. Presets for game-specific profiles (GH, RB, etc.). Foundation for everything downstream.
- **Section borders + event markers** — EVENTS track parsing, blue measure lines, section name overlay. Backend partially wired (`TimeBasedEventMarker`, `populateEventMarkers`). Tempo/timesig marker infrastructure landed (`d23505b`), rendering disabled. Enable for tempo/timesig, then extend for sections, lyrics.
- **Multi-highway: missing track types** — 6-fret variants still missing (RHYTHM GHL, COOP GHL, KEYS GHL). 5-fret COOP/RHYTHM now have separate Part enum values.
- **Ratio-agnostic window resizing** — Viewport-based resize landed (`c030e7d`), but "too much space below strikeline" tuning and REAPER fullscreen restore white gap / ~98% size bug still open.
- **SysEx marker support** — Read Phase Shift SysEx events for open/tap note markers. The open note marker turns ALL notes into opens, not just greens. See [chart format spec](https://thenathannator.github.io/GuitarGame_ChartFormats/Chart-File-Formats/mid-format/Format-Overview/#phase-shift-sysex-event-specification).
- **Video export** — Render chart playback as video for YouTube previews. Multiple community requests. Could be a standalone tool or a feature in the standalone build. Separate project scope.
- **Unglued MIDI items show overlapping notes** — Reported by Easyfan: split/unglued MIDI items in REAPER render notes from multiple sections at once. Gluing fixes it. Unclear whether this is a bug in the plugin's MIDI fetching (reading across item boundaries) or expected REAPER behavior (unglued items expose all takes). Needs investigation.
- **Live MIDI edits not reflecting** — Reported by Galexio: HOPO changes not updating without removing/re-adding plugin. Could not reproduce. Unclear if this is a REAPER MIDI cache staleness issue, a plugin polling gap, or user error. May be fixed by 1.2's live track detection. Monitor for further reports.
- **Native framerate caps at 60Hz on multi-monitor Windows** — Reported by Monty: primary 144Hz + secondary 60Hz. Native FPS setting caps at 60 instead of 144. Regression from `f176e65` (Timer → VBlankAttachment). Root cause: JUCE 8's `VBlankAttachment` binds to the peer's monitor at creation via `MonitorFromWindow`; a 1-second `monitorUpdateTimer` re-checks but only when `parentToAddTo != nullptr`. When a plugin opens on the secondary display, it can get stuck on 60Hz even after moving to 144Hz. Not a quick fix — options are (a) revert to `juce::Timer` (loses display sync), (b) dual-path Timer+VBlank, (c) document as known limitation. Needs a Windows machine to verify any fix. Single report so far.
- **Non-REAPER DAW support broken** — Standard pipeline (ChartchoticStd) doesn't work in Ableton as of 1.2. Plugin loads but no functionality. Needs investigation into what the standard MIDI pipeline is actually missing without REAPER APIs.
- **Standalone mode** — Proper standalone build that loads a chart/MIDI file, displays the highway, and supports video export. Pairs with video export and standalone chart loading items.
- **Audio playback** — Metronome click, guitar note sounds, drum hit sounds for listening back to charts. Assets mostly ready. Needs audio output from plugin + sync.
- **Standalone chart loading** — Open a chart file/folder and play it back without REAPER. Debug standalone already has playback; needs file open UI, chart format parsing, tempo map from file. Pairs with audio playback.
- **Update banner: show changelog** — `releaseNotes` already fetched from GitHub release body (release channel). Need: display in overlay card (taller/scrollable), strip markdown to plaintext, also fetch for dev channel.
- **Solo sections** — Blue highway background during solo passages.
- **Drum fills / BRE** — Full lanes for kicks/open, activation gem logic.
- **Better mouse scrolling** — shift=faster, ctrl=precise.
- **Info display** — BPM, time sig, measure, beat position.
- **Disco ball animation upgrade** — Replace static disco ball JPG with rotating disco ball (quarter-turn loop), "DISCO FLIP" text underneath in disco/rainbow colors. Fun polish.
- **Write-mode UI cleanup pass (post-authoring)** — Bundled visual + UX polish after M3 / M4 land, before `1.3.0` ship. By then all authoring visual elements (hover ghost, drag preview, selection highlight, erase indicator, marquee) exist and the cohesive look can be designed in one pass. Collected items:
  - **Mode chip stability**: floating icon currently shifts when sub-toolbar appears/disappears. Should hold a fixed position regardless of sub-toolbar visibility. Also conflicts with instrument/difficulty selection — need a placement that doesn't interfere.
  - **Bring back the explicit DRAW/EDIT label** — sub-toolbar wash is good but a literal label was clearer at a glance.
  - **Mouse cursor glyph** — currently doesn't read well, looks shifted left in its frame, not centered. Needs a better arrow shape or a different "edit" metaphor.
  - **Tuplet "off" coloring** — should be `textWhite` like division (it's a valid setting, not a disabled state). Currently dim, indistinguishable from snap-disabled fade.
  - **Snap as checkbox / toggle** — pill aesthetic implies clickability we don't have yet. Consider checkbox or pure label, possibly with a transient toast ("Snap Enabled" / "Snap Disabled") when key shortcuts fire to give keyboard-only users feedback.
  - **Note-icon notation for division/tuplet** — could replace "1/8" / "Tuplet: 3" text with proper musical notation glyphs (like MuseScore). Tradeoff: more polished but more rendering work.
  - **Sub-toolbar height jump between Draw and Edit** — toolbar height changes between modes; should not. Want zero reflow when toggling between draw/edit (only visibility changes).
  - **Toolbar width** — currently full-width strip; consider centered, only-as-wide-as-needed. TBD.
  - **Default gridlines in write mode** — when write mode is on, ONLY the user-chosen grid should render (plus measure anchors). Currently the existing MEASURE/BEAT/HALF_BEAT all keep rendering, masking the user's selected step+tuplet grid (visible bug: 1/4 + tuplet=3 shows the default 8th-note grid drowning out the actual triplet lines). This may bite M3 usability; revisit if so during M3.
  - **Division/tuplet broken below 1/8** — likely same root cause as above.
- **Dedicated STEP gridline asset** — Currently aliased to `marker_half_beat_png`, differentiated only by opacity (STEP 0.25 vs HALF_BEAT 0.35). Add a real `marker_step.png` if a thinner / more subtle visual is wanted; register in BinaryData + AssetManager. Don't reinvent rendering paths.
- **Highway time-position column (glass panes)** — Optional column on the right side of the highway showing musical position per gridline (measure number, beat number, time). Like a row of glass panes alongside the chart. Mostly useful in write mode for orienting against REAPER's timeline. Probably needs perspective-aware text rendering (text scales with depth) which the renderer doesn't currently do — non-trivial.
- **Write-mode gridlines on top of bars** — Currently MEASURE/BEAT render at `DrawOrder::GRID` (below `BAR`), so bar gems (kicks/opens) cover the rhythmic structure. Multi-pass alpha boost (`5ad78b3` follow-up) helps prominence in non-bar zones but doesn't solve occlusion. Option: in write mode, emit MEASURE/BEAT at a higher draw order (above `BAR`, below `NOTE`) so structural anchors cut through bars like in any MIDI editor. Easy enum addition + per-emit override.

### Done (1.2)

**Bemani mode:**
- ~~Flat scrolling viewport~~ — Sidebar rails, per-instrument tuning, data-driven debug panel, BemaniConfig refactor.
- ~~Texture scroll snap~~ — Removed fmod wrapping from scroll offset (both REAPER and standalone paths).
- ~~Disco flip force field markers~~ — Flat horizontal markers in Bemani mode.
- ~~Sidebar edge lines~~ — Sidebar rails via DrawCallMap at TRACK_SIDEBARS draw order.
- ~~Unified note speed~~ — Both modes share 2-20 range, Bemani applies internal 1.7x ratio.
- ~~Pre-release fixes~~ — Per-highway padding, highway length scales with slot height, force horizontal row in multi-highway.

**Multi-highway:**
- ~~Pool HighwayComponents + shared track image cache~~ — 4 pooled slots (never destroyed), TrackImageCache bakes guitar+drums once at full resolution. Difficulty/instrument toggles swap overlay pointers (~0ms).
- ~~Deferred gem type calculation (v2)~~ — Zero-stutter: NoteData stores velocity only, TrackResolver computes gems at render time. Toggle changes instant.
- ~~Batched data build~~ — Extract+resolve once per lock group instead of per-slot.

**Performance:**
- ~~Async track image rebake~~ — Background thread bake with generation counter. Stale cache stretches during drag/resize, snaps on completion.
- ~~Profiling infrastructure~~ — Per-frame TSV logger, per-highway phase timing, pipeline benchmark target. Profiler lifecycle encapsulated in DebugEditorController.

**Other:**
- ~~Disco flip (Pro Drums)~~ — Text event infrastructure + DiscoFlipState + red↔yellow swap. Force field markers at start/end positions.
- ~~Update overlay improvements~~ — ESC dismisses, resizes with window.
- ~~Lane/sustain draw order~~ — SUSTAIN before BAR (bars render on top).
- ~~Source tree reorg~~ — Extract Editor/ module from PluginEditor.

**Multi-highway:**
- ~~Track re-discovery~~ — `pollForChanges` re-runs discovery, detects added/removed/renamed tracks live.
- ~~GUITAR_COOP + RHYTHM parts~~ — Separate Part enum values, own highway slots, centralized track name constants.
- ~~Remove Global/Local toggle~~ — Always use global discovery in REAPER; non-REAPER uses manual fallback.
- ~~No-tracks-found message~~ — Shows supported track names when REAPER has no instrument tracks.
- ~~Dedup duplicate parts~~ — First match per instrument type wins in discovery.

**Bug fixes:**
- ~~Drum track cache overflow~~ — Per-instrument overflow in TrackImageCache instead of shared max.
- ~~REAPER mode latch~~ — Toolbar stays in REAPER mode when API flickers during track moves.
- ~~Editor re-attach~~ — Rebuild session state when editor is recreated (plugin moved between tracks).

**Closed (not needed):**
- ~~Resolve-per-track caching~~ — Already batched per lock group in FrameDataBuilder; `resolveAllDifficulties` runs once, each slot extracts its difficulty.
- ~~ReaperMidiPipeline slimming~~ — Anticipated duplication never materialized; functions only exist in pipeline.

---

## Tech Debt

Do between features or when touching related code.

- **Stretch + Bemani mode interaction** — Stretch-to-fill doesn't work when Bemani mode is active. `onBemaniModeChanged` doesn't account for stretch state, and `resized()` computes `sceneHeight` differently in Bemani mode regardless. Preexisting.
- **NoteStateStore wrapper** — `InstrumentSlot` bundles array+lock but doesn't enforce locking via API yet. Wrap into class with scoped-lock accessors. Eliminates race condition footgun.
- **Audio-thread hygiene** — Remove std::function, preallocated vectors. DrawCallMap done, remaining allocations TBD.
- **Minimize REAPER API calls** — Profiler/DrawCallMap work done, REAPER call frequency still unaudited.
- **Settings menu reorganization** — Single gear button, scrollable category list on the right, selected category expands a detail panel on the left (macOS System Settings style). Consolidate View/Chart and Settings panels into one unified menu. Centralize control labels and tooltip text into a single source of truth file.
- **Tooltips for all controls** — Extend InfoTooltip to every control that isn't immediately obvious. Centralize tooltip strings (header with static constexpr or similar) so they're easy to maintain and translate.
- **Auto-detect stale CMake build dirs** — `build.sh`, `build-reapertest.sh`, and `scripts/test.sh` should compare `CMAKE_HOME_DIRECTORY` from any existing `CMakeCache.txt` against the current script dir and auto-`rm -rf` the build tree on mismatch before configuring. Bit us twice in one session after moving the repo from `plugins/` to `charting/`.
- **`build-reapertest.sh` should force-quit REAPER** — The "Quitting REAPER..." step hangs when REAPER has unsaved-changes or end-of-session prompts open, forcing a manual Ctrl-C. Replace graceful quit with `kill -9` / `killall -9 REAPER` so the relaunch step always makes progress.

### Rendering — perspective limits (do not iterate further without a true-3D plan)

**True-3D pipeline rewrite (parking-lot / future major version).** The fake-3D trapezoid in `PositionMath::createPerspectiveGlyphRect` runs two independent 1/z curves (one for X/Y/width, one for sprite height). Any "above the bar plane" element therefore can't keep a constant gap-to-sprite ratio across depth without distorting aspect or breaking lane fit. Two attempted reworks (Apr 11–12 `9128b93`+`49f71a0` and the Apr 20–21 lift-mode/crosshair session) both hit the same wall and have been reverted. Full postmortem in `docs/Z_POSITIONING.md`. Experimental code on `experiment/z-position-rework`. **Do not attempt another scalar-tweak fix.** The right path is a real view-projection matrix.

The following items are subsumed by that rewrite and only worth fixing in isolation if they bite for some specific reason:
- `PositionMath.cpp:99` — `progress` goes negative past `vanishingPointDepth (1.0)` while `HIGHWAY_POS_END = 1.12` and `FAR_FADE_DEFAULT = 1.20` render past that. Slight extrapolation artifacts at the far end.
- `NoteRenderer.cpp` guitar gem branch — reads `sNear/sFar/w/h` from per-column adjusts but never adds `ca.z` to zOff (drum branch does). All guitar Z values are 0 anyway, so no visible impact.
- `SustainRenderer.cpp` — uses raw lane `centerY` with no Z nudge, so guitar notes (when `gemZ` is non-zero) are vertically disconnected from their sustain tails.
- `getFretboardEdge` / `getColumnPosition` accept `posStart`/`posEnd` but ignore them. Highway-end tuning doesn't reach perspective math.

---

## Test Coverage

Current: 58 tests covering GemCalculator (20), InstrumentMapper (15), GridlineGenerator (11), MidiInterpreter (10), smoke (2). All pure functions, no JUCE GUI or REAPER API needed.

### Critical (add before any HOPO/sustain/disco flip refactor)

**TrackResolver note resolution** (`Midi/Processing/TrackResolver.cpp:133-251`)
- Auto-HOPO threshold: test PPQ distances at exact boundary (e.g., 170 ticks = HOPO, 171 = strum). Each threshold option (1/16, Dot 1/16, 170 Tick, 1/8) needs a boundary test.
- Chord detection: 2+ notes at same PPQ forces strum regardless of distance. Test single note after chord (should allow HOPO), chord after single (should force strum).
- Disco flip cymbal swap: when `discoFlip=true` and `proDrums=true`, red cymbals become yellow and vice versa. Test all gem types through the swap (GEM::CYMBAL stays cymbal, column changes). Test that non-cymbal notes are unaffected. Test flip disabled = no swap.
- Pro drums cymbal detection: velocity 110+ on tom pitch = cymbal. Test boundary (109 = tom, 110 = cymbal). Test with proDrums disabled = all toms regardless of velocity.

**TrackResolver sustain resolution** (`TrackResolver.cpp:257-296`)
- Duration filtering: sustains shorter than `MIDI_MIN_SUSTAIN_LENGTH` should be dropped. Test at exact boundary.
- Gem type at sustain start: sustain inherits the gem type of its starting note (HOPO/TAP/STRUM). Test sustain starting on a forced HOPO note vs strum note.
- Star power state: sustain that starts outside SP but the note-on is inside SP should be marked SP. Test boundary where SP region starts/ends mid-sustain.
- Skill filtering: sustains only appear for the correct difficulty range. Test Expert sustain doesn't leak into Hard window.

**TrackResolver lane resolution** (`TrackResolver.cpp:302-357`)
- Skill filtering: Expert always sees lanes. Hard sees lanes only with velocity 41-50. Medium/Easy never see lanes. Test each.
- Lane column discovery: LANE_1 allows up to 2 columns, LANE_2 allows up to 1. Test with notes spanning multiple columns.

**DiscoFlipState text parsing** (`Midi/DiscoFlipState.h`)
- Valid events: `[mix 3 drums0d]` (flip on, Expert), `[mix 3 drums0]` (flip off). Test all 4 difficulties (0-3).
- `dnoflip` suffix: `[mix 3 drums0dnoflip]` ends a flip region. Test that it properly closes the region.
- Malformed rejection: `[mix]`, `[mix 3]`, `[mix 3 guitar0d]`, `[mix 99 drums0d]` should all be ignored.
- Unterminated flip: if flip-on never gets flip-off, region extends to PPQ 999999.0. Test this edge case.
- Multiple difficulties: flip on for Expert only should not affect Hard. Test per-difficulty isolation.
- Overlapping regions: flip-on, flip-on, flip-off. Second on should be ignored (already flipping).

### High (add before NoteProcessor or discovery changes)

**NoteProcessor boundary conditions** (`Midi/Processing/NoteProcessor.cpp`)
- Note-off placement: `max(start + PPQ(1), end - PPQ(1))`. Test zero-length note (start == end), 1-tick note, normal note. The formula prevents note-off from being before or at note-on.
- Pitch bounds: pitch >= `array.size()` should be silently skipped. Test with pitch 128 (out of range) and pitch 127 (max valid).
- Velocity passthrough: NoteData should store the original MIDI velocity. Test that velocity 0 = note-off, velocity 1-127 = note-on with correct value.
- Muted note filtering: notes with `muted=true` from MidiCache should be skipped entirely.

**matchTrackNameToPart** (`Midi/Discovery/TrackDiscovery.h`)
- Case insensitivity: "part guitar", "PART GUITAR", "Part Guitar" should all match.
- All implemented names: test every entry in `getImplementedTrackNames()` maps to the correct Part.
- All unimplemented names: test every entry in `getUnimplementedTrackNames()` maps correctly.
- Unknown names: "PART SOMETHING", "GUITAR", empty string should return false.
- Consistency: every name in getImplementedTrackNames/getUnimplementedTrackNames should successfully match via matchTrackNameToPart.

**pollForChanges structure detection** (`Midi/InstrumentSession.cpp`)
- Track added: start with 2 tracks, re-discover finds 3. Should return true, trackData resized.
- Track removed: start with 3 tracks, re-discover finds 2. Should return true.
- Track renamed: same count but different Part on one track. Should return true.
- Track reordered: same parts but different sourceTrackIndex. Should return true.
- No change: same tracks, same parts, same indices. Hash unchanged = return false.
- Note edit only: same structure, different hash on one track. Should return true, only that track refetched.
- All tracks removed: re-discover returns empty. Should return true.

### Low (add when touching related code)

**ControlConstants helpers** (`UI/ControlConstants.h`)
- getRenderType: test every Part enum value returns expected RenderType. Catches missing switch cases when new Parts are added.
- isGuitarLike/isDrumLike: test all Parts, verify consistency with getRenderType.
- isHighwayRenderable: test all Parts. VOCALS, PRO_KEYS etc should return false.
- getPartSortOrder: test that Guitar < Rhythm < Coop < Bass < Keys < Drums. No duplicates.
- getImplementedTrackNames: every entry's Part should be highway-renderable.

### Test infrastructure notes

- All candidates are pure functions: data in, data out. No JUCE GUI, no REAPER API, no audio thread.
- Test CMakeLists (`tests/unit/CMakeLists.txt`) links specific .cpp files. When adding TrackResolver tests, it's already linked. For DiscoFlipState, it's header-only so no additional linking needed.
- NoteProcessor.cpp needs to be added to the test CMakeLists link list.
- TestFixture in `test_helpers.h` provides NoteStateMapArray + ValueTree setup. Extend it for TrackResolver tests with SharedWindow/Config builders.
- For pollForChanges tests, InstrumentSession needs a mock TrackDiscovery and mock TrackNoteProvider. These are simple interfaces (1-2 methods each) so mocking is straightforward.

---

## Future

Unordered. Pull into Up Next when the time comes.

**Chart Features:**
- Instrument autodetection (by track name — depends on EVENTS parsing)
- Note color customization (CH color profile templates)
- GH style gems toggle
- Star Power rendering (partially working, architectural approach unclear — finish or remove)

**Real Drums:**
- Flam note types, HH open/closed/foot gems
- Generic gem system (color any part of note)
- Reorderable lanes + custom colors
- 5-lane drums, 8-lane drums
- Refactored MIDI parsing (supports reductions, benefits all instruments)

**Authoring & Vocals (way out):**
- Moonscraper-style note placement, grid snapping, note eraser
- INI generation/export
- 2D pitch-based karaoke display, lyrics with rhythm timing

**DAW Integration:**
- Max4Live device for Ableton — proper Ableton support via Max4Live to expose REAPER-equivalent features (text events, timeline access, track discovery) through Ableton's LOM. Would enable disco flip, sections, multi-highway, etc. The path to full Ableton support since the standard plugin pipeline can't access what it needs without DAW-specific APIs.

**Architecture (do when it hurts):**
- AudioProcessorValueTreeState migration
- Double-buffered snapshots for renderer
- **Pop-out highway windows (one plugin, many viewports)** — Today "two windows" = two plugin instances on the same track = duplicated REAPER API traffic, duplicated polling, duplicated InstrumentSession. Replace with "Pop out" action in the toolbar that spawns a `juce::DocumentWindow` hosting an additional `HighwayComponent` off the same `SessionController`/`SlotData`. One processor, one session, one REAPER poll — N viewports. Makes the "6 instances on one track" perf cliff go away by removing the reason to do that. Side-steps the shared-project-scan-cache design (not needed if there's only one consumer). Leaves the real multi-instance case (independent tracks) as the only path where cross-instance caching would matter, which is rare enough to defer indefinitely. REAPER duplicate-track hang (fixed in `fix/reaper-duplicate-hang`, commit `da1d115`) was the forcing function.

---

## Icebox

Blocked or no clear path forward.

- **#17 — Linux REAPER scan failure** — May be resolved by cross-platform build fixes. Needs verification. [GitHub](https://github.com/noahbaxter/chartchotic/issues/17)
- **#16 — FL Studio: no notes appear** — *Blocked: don't own FL Studio.* [GitHub](https://github.com/noahbaxter/chartchotic/issues/16)
- **#3 — Logic: AU loads as Audio FX, no MIDI** — *Blocked: don't own Logic.* [GitHub](https://github.com/noahbaxter/chartchotic/issues/3)
- Elite Drums (beyond Real Drums)
- Extended memory for standard pipeline (notes visible when transport stops)
- REAPER auto track detection
- REAPER preset integration (.ini files)
- .ini file generation (community suggests CAT instead)
- Note length validation (possible ReaScript utility)
- Motion sickness mitigation (unclear solution)

---

## Notes

- **Moonscraper overlap**: Community consensus is Chartchotic is for preview, not charting.
- **Key dependency chain**: EVENTS parsing -> section detection -> autodetection -> Real Drums MIDI refactor -> generic gem system
- **Community feedback source**: Discord #chartchotic channel
