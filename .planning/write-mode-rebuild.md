# Write Mode — Rebuild Plan

**Branch:** `wip/authoring`
**Base:** `8dac3de` (v1.2.3 main + foundation cherry-picks + texture fix)
**Old work:** preserved at tag `archive/old-wip-authoring` for code reference
**Spec:** `docs/WRITE_MODE.md` (behavior contract)
**This doc:** the implementation plan — files, integration points, risk, validation, decisions

---

## Status

Foundation only. Mode toggle and editing not yet implemented on this branch. Old branch had it built on top of a painter refactor that conflicts with v1.2.3's Frame system, so we rebuild fresh against current architecture.

---

## What's already on the branch (foundation, do not redo)

| Component | Commit | Purpose |
|---|---|---|
| REAPER write API function pointers | `218dc32` | `Undo_BeginBlock2`/`EndBlock2`, `MIDI_InsertNote`, `MIDI_DeleteNote` etc. wired in `ReaperApiHelpers.h` |
| `MidiWriter` interface | `c8803be` | DAW-agnostic write/erase/undo-block contract |
| `ReaperMidiWriter` | `c8803be` | REAPER impl of `MidiWriter` |
| `HitTestMapper` | `e81ea1f` | Screen pixel → `(time, lane, pitch)` accounting for perspective + curvature |
| Plugin GUI thread undo | `71124cf` | Ensures REAPER undo blocks work when MIDI is written from editor (not audio) thread |
| `gothic_default` highway texture | `8dac3de` | Default texture (your fix) |

---

## Architecture: how the pieces fit (post-painter)

```
                  ┌──────────────────────────┐
                  │     PluginEditor         │
                  │  - owns WriteController  │
                  │  - handles W/Q/[/]/T/S   │
                  │    keys                  │
                  └────────────┬─────────────┘
                               │
                  ┌────────────▼─────────────┐
                  │    WriteController       │
                  │  - mode state (Draw/Edit)│
                  │  - selection             │
                  │  - step/snap/tuplet      │
                  │  - placeNote/eraseNote   │──┐
                  │  - selection ops         │  │
                  └────────────┬─────────────┘  │
                               │                │
                  ┌────────────▼─────────────┐  │
                  │   HighwayComponent       │  │
                  │  - mouse events          │  │
                  │  - paints ghost cursor   │  │
                  │  - paints selection rect │  │
                  │  - paints drag preview   │  │
                  └────────────┬─────────────┘  │
                               │                │
                  ┌────────────▼─────────────┐  │
                  │    HitTestMapper         │  │
                  │  - pixel → (ppq, lane,   │  │
                  │    pitch)                │  │
                  └────────────┬─────────────┘  │
                               │                │
                  ┌────────────▼─────────────┐  │
                  │      MidiWriter          │◄─┘
                  │  (ReaperMidiWriter impl) │
                  │  - insertNote            │
                  │  - deleteNote            │
                  │  - beginUndoBlock        │
                  │  - endUndoBlock          │
                  └──────────────────────────┘

  NoteRenderer / SustainRenderer (existing, v1.2.3)
  - take optional writeMode flag for opacity / clip behavior
  - expose helpers needed by ghost cursor (curved cache,
    column position math) — already public via PositionMath
```

**Key invariant:** `WriteController` is the single owner of write-mode state. `HighwayComponent` translates mouse events into `WriteController` calls. `WriteController` calls `MidiWriter` which writes to REAPER. Renderers read `writeMode` flag passively (for visual changes) but never own state.

**No painter classes.** Drag preview, ghost cursor, and selection visuals render directly via `juce::Graphics` in `HighwayComponent::paintOverChildren`, using the existing `PositionMath` / `NoteRenderer` helpers for geometry.

---

## Phase 1 — Mode infrastructure

**Ship target:** `1.3.0-alpha` ("write mode toggles, no editing yet")

### Behavior

- `W` toggles write mode on/off (any context)
- `Q` toggles Draw/Edit (only when write mode on)
- Mode persists across enter/exit of write mode
- Write mode disabled during playback — interactions blocked, but state persists for when playback stops
- `[` halves step division, `]` doubles
- `T` cycles tuplet (off → 3 → 5 → 7 → off)
- `S` toggles snap
- Mode pill: existing "EDIT" pill expands when write mode active to show "DRAW" or "EDIT" in distinct colors
- Step gridlines: render with brighter writeMode opacity, includes new STEP gridline type

### Files

**Create:**
- `Chartchotic/Source/Editor/WriteController.{h,cpp}` — port the structure from `archive/old-wip-authoring:Chartchotic/Source/Editor/WriteController.{h,cpp}`. API mostly correct, no painter dependencies in WriteController itself.

**Modify:**
- `Chartchotic/Source/PluginEditor.cpp` — instantiate `WriteController`, wire `W`/`Q`/`[`/`]`/`T`/`S` keypresses
- `Chartchotic/Source/PluginEditor.h` — `WriteController writeController` member
- `Chartchotic/Source/UI/ToolbarComponent.cpp` — mode pill expand-with-color logic
- `Chartchotic/Source/UI/ToolbarComponent.h` — mode pill API
- `Chartchotic/Source/Visual/Renderers/GridlineRenderer.cpp` — writeMode opacity branch (already documented in `WRITE_MODE.md`, was in old branch)
- `Chartchotic/Source/Visual/Renderers/GridlineRenderer.h` — `bool writeMode = false;` member
- `Chartchotic/Source/Visual/Managers/GridlineGenerator.h` — STEP gridline generation per current step/tuplet
- `Chartchotic/Source/Utils/ChartTypes.h` — `Gridline::STEP` enum value
- `Chartchotic/Source/Visual/Managers/AssetManager.cpp` — STEP gridline asset (might be alias of HALF_BEAT?)

### Integration points

- `WriteController::init(processor, state, highway)` is called from `PluginEditor` after components exist
- `WriteController::toggle()` and `toggleMode()` are called from `PluginEditor::keyPressed()`
- `WriteController::update(isPlaying)` is called per frame so mode state can react to playback transitions
- `GridlineRenderer::writeMode` is set by `SceneRenderer::paint()` based on `WriteController::isActive()`

### Risk

- **Low.** Mostly straight ports. The mode pill UI change is the only design-side risk — needs to look right in both states.
- **Medium:** STEP gridline generation needs to interact with current tuplet and step setting. The generator math from old branch should port.

### Validation

- Unit: WriteController state machine — toggle, mode toggle, step halve/double bounds
- Integration: visual — mode pill color changes, step gridlines appear/brighten when entering write mode
- Manual: open plugin → press W → see pill change, press Q → see DRAW/EDIT switch, press `[`/`]` → grid gets denser/sparser

### Definition of done

- `W`/`Q`/`[`/`]`/`T`/`S` work
- Mode pill shows DRAW or EDIT in write mode, EDIT in non-write mode
- Step gridlines render correctly at all step sizes including tuplets
- No interactions yet (clicking does nothing different)
- Build green, plugin loads, no crashes

---

## Phase 2 — Draw mode core (place / erase / sustain drag)

**Ship target:** `1.3.0-beta` ("can write basic charts")

### Behavior

- Left-click empty cell → place short note at grid position
- Right-click on note → erase
- Left-drag → sustain placement (drag forward in time = sustain, drag down/sideways = lane change)
- Drag distance threshold ~3px to disambiguate click from drag
- Sustain end snaps to grid
- Minimum sustain length applies
- Sustain drag polish:
  - Lane follows cursor X in real-time during drag
  - Cascade: dragging into existing note pushes it back
  - Snap-to-next-note: drag end snaps to next note start if close
  - Existing-note adjustment when overlap occurs
- Hover ghost cursor (basic, full curvature/z polish in Phase 5)
- Drag preview:
  - Note head at start position (ghost gem)
  - Sustain tail from start to current cursor (if dragging forward)

### Files

**Modify (no new files for core):**
- `WriteController.cpp` — `placeNote`, `eraseNote`, sustain drag commit logic
- `HighwayComponent.cpp` — mouse event handlers, drag state tracking, drag preview painting
- `HighwayComponent.h` — drag state members
- `SustainRenderer.{h,cpp}` — add public `drawSinglePreviewSustain(g, lane, startPos, endPos, ...)` for drag preview tail (replaces the LanePainter call from old branch)
- `NoteRenderer.{h,cpp}` — make `getCurvedImage(image, column, isDrums)` public so drag preview gem can match real rendering curvature

### Integration points

- `HighwayComponent::mouseDown`: in Draw mode, capture start position via `HitTestMapper`, set drag state on `WriteController`
- `HighwayComponent::mouseDrag`: compute cursor position via `HitTestMapper`, update drag state, trigger repaint
- `HighwayComponent::mouseUp`: if drag ≥ threshold, call `WriteController::commitSustain(start, end, lane)`; else call `WriteController::placeNote(start, lane)`
- `WriteController::commitSustain`: applies cascade / snap / overlap logic, calls `MidiWriter::insertNote` inside undo block
- Drag preview rendering: `HighwayComponent::paintOverChildren` reads drag state from `WriteController`, paints ghost gem + sustain tail

### Risk

- **High:** sustain drag polish (cascade, real-time lane, existing note adjustment) is the most behavior-dense logic on the old branch. Concentrated in `30e76d4`, `afc31e0`, `11d9d0e`, `686cb8b`. Must be ported correctly or sustain drag feels wrong.
- **Medium:** Drag preview rendering. Old code used `LanePainter::paint` with full transform replication. New approach: expose a `SustainRenderer::drawSinglePreviewSustain` entry point that takes pre-projected coordinates and emits draw calls, called from `HighwayComponent::paintOverChildren` with the same transform logic the existing `paint()` uses. Keep within the existing transform stack — don't replicate the whole transform from scratch.
- **Low:** click/drag threshold disambiguation — well-understood pattern.

### Validation

- Unit: WriteController place/erase/sustain logic — correct PPQ snap, lane mapping, undo block boundaries
- Unit: HitTestMapper — round-trip pixel → music → pixel within ~1px
- Manual: place 100 notes by clicking, all land where expected at varying step sizes
- Manual: drag a sustain into an existing note, confirm cascade pushes it back
- Manual: drag ends near another note, confirm snap-to-note works
- Manual: drag while changing lanes mid-drag, confirm final lane is at release X
- Manual: undo single click, undo full drag — each is one action

### Definition of done

- Click places a short note exactly on snap (or nearest 1/128 if snap off)
- Right-click erases the note under cursor
- Drag commits a sustain that obeys cascade and snap-to-next-note
- Drag preview is visible during the gesture (gem + trail)
- Single click vs. drag is disambiguated correctly
- Each gesture is one undo

---

## Phase 3 — Multi-note Draw ("MS-killer")

**Ship target:** `1.3.0` (with Phase 2) or `1.4.0` (separately)

### Behavior

- Shift+left-drag = paint short notes along path, one per time step
  - One per row constraint (no chords from a single stroke)
  - Replaces if revisited within same gesture
  - Pre-existing notes never affected (skip cell, don't replace)
  - Forward and backward painting
  - Whole stroke = one undo
- Right-drag = erase sweep
  - Removes any note the cursor crosses
  - Whole sweep = one undo
- Visual: shift held shows multi-ghost (2-3 progressively faded ghosts trailing down highway)
- Visual: right-click shows X overlay on ghost

### Files

**Modify:**
- `WriteController.cpp` — paint stroke and erase sweep state machines
- `HighwayComponent.cpp` — modifier key tracking, multi-ghost rendering, X overlay rendering

### Integration points

- `WriteController::beginPaintStroke()` / `addPaintCell()` / `endPaintStroke()` — track placed cells, dedupe within gesture
- `WriteController::beginEraseSweep()` / `addEraseCell()` / `endEraseSweep()` — track erased notes, dedupe within gesture
- Multi-ghost rendering reads modifier state + drag state from `WriteController`

### Risk

- **Medium:** paint dedupe semantics — "replace if revisited within gesture" needs careful state tracking. Old commit `d9bd1cf` had it working; reference for behavior.
- **Medium:** undo block scoping — the whole stroke is one undo. Must `beginUndoBlock` on first paint cell, `endUndoBlock` on stroke end, and any cancellation must cleanly close the block.
- **Low:** rendering — multi-ghost is just additional ghost-cursor calls at offset positions.

### Validation

- Manual: shift+drag across 10 cells, all land at correct time steps with one undo
- Manual: shift+drag back across already-painted cell, confirm replacement
- Manual: shift+drag through a pre-existing note, confirm it's untouched
- Manual: right-drag through 5 notes, all erased with one undo
- Manual: hold shift, see multi-ghost; right-click, see X overlay

### Definition of done

- Shift-paint works for forward and backward strokes
- Right-drag-erase removes all crossed notes
- Multi-ghost visual appears with shift held
- X overlay visual appears with right-click
- Each gesture is one undo

---

## Phase 4 — Edit mode multi-select

**Ship target:** `1.4.0`

### Behavior

- Click on note in Edit mode = select (clears prior selection)
- Shift+click = add/remove from selection
- Click empty area = clear selection
- Drag from empty area = marquee select
- Marquee visual: perspective-matched trapezoid (wider near strikeline)
- Selected notes: time-shift via Up/Down (atomic, replaces collisions)
- Selected notes: lane-shift via Left/Right (no wrap, edges stop)
- Delete / Backspace: removes selection
- `Cmd+A`: select all visible (currently visible window only)
- Each move/delete = one undo

### Files

**Modify:**
- `WriteController.cpp` — selection set (multi), shift-toggle logic, group ops
- `HighwayComponent.cpp` — marquee state and rendering, selected note highlighting

**Possibly create:**
- `Chartchotic/Source/Visual/Utils/MarqueeGeometry.{h,cpp}` — perspective trapezoid math, hit testing notes against trapezoid in screen space

### Integration points

- `WriteController::selection` becomes `std::set<NoteRef>` instead of single
- `WriteController::shiftSelection(deltaTime)` and `shiftSelection(deltaLane)` for arrow keys
- `WriteController::deleteSelection()` for Delete key
- Marquee active during left-drag from empty position; HighwayComponent owns marquee rect, asks WriteController to commit selection on release

### Risk

- **High:** marquee perspective trapezoid was never built. Geometry: at any vertical position on the highway, the lane width and angle differ. Trapezoid corners are not just (x1,y1) (x2,y2) but need to follow the highway's projected lane lines. Need to test against real notes' projected positions — `PositionMath::getColumnPosition` gives the per-note projection. Marquee hit test = "does this note's projected center lie within the marquee polygon?"
- **Medium:** atomic multi-note move with collision replacement. Old `aa2d8ac` commit had this. Reference behavior, port logic.
- **Low:** selection set state is straightforward.

### Validation

- Unit: marquee geometry — given start/end pixel coords, compute trapezoid corners that match highway projection
- Unit: hit test — note in trapezoid → selected
- Manual: marquee select 5 notes, see them highlight
- Manual: shift+click to add/remove, confirm count changes correctly
- Manual: arrow-shift selection through other notes, confirm collisions replace and undo restores

### Definition of done

- All Edit mode interactions per spec work
- Marquee feels right (trapezoid actually matches what user expects to grab)
- Multi-select operations are atomic in undo

---

## Phase 5 — Visual polish (hover ghost cursor)

**Ship target:** ideally bundled with Phase 2 (placement feels broken without it)

### Behavior

- Hover ghost cursor shows note shape at cursor position
- Curvature applied (matches actual note rendering — uses cached curved variant)
- Z-offset alignment (ghost sits on lane plane correctly)
- Bar position offset for kick lanes
- Guide line glow (vertical line from cursor down highway, visualizes time alignment)

### Files

**Modify:**
- `HighwayComponent.cpp` — `computeNoteOverlay()` for ghost cursor geometry
- `NoteRenderer.{h,cpp}` — make `getCurvedImage(image, column, isDrums)` public for ghost rendering
- `HighwayComponent::paintOverChildren()` — paint ghost cursor with same image/scale logic as real notes

### Integration points

- `HighwayComponent::mouseMove` updates hover position via `HitTestMapper`
- `paintOverChildren` reads hover position, calls `NoteRenderer::getCurvedImage` for matching ghost gem

### Risk

- **Medium:** the ghost cursor needs to match real note rendering exactly (curvature, scale, Z-offset, opacity) or it'll feel off. Old branch's `f02f54d` had this dialed; current branch's `NoteRenderer` has the same math but reorganized.
- **Low:** guide line is just a vertical line.

### Validation

- Manual: hover near a real note, ghost cursor matches its visual exactly (same size, curvature, position)
- Manual: hover bar lane, ghost cursor matches kick bar size/position

### Definition of done

- Ghost cursor matches real notes pixel-for-pixel under same conditions
- No visual drift between ghost and where the note actually lands when clicked

---

## Phase 6 — Power tools (deferred to 1.4.x / 1.5.x)

Ship as patches after 1.3.0 stable:

- Keyboard step input `0-5` (place note in lane at playhead)
- Kick/open lane filter `O` (visual fade + interaction constraint)
- 2x kick batch `K` (alternate every other selected kick)
- Copy/paste `Cmd+C` / `Cmd+V` (cursor-aligned, relative spacing preserved)
- Sub-toolbar UI (step stepper, snap toggle, triplet toggle, kick filter, mode toggle as visible controls)
- Configurable keybinds (Phase 5 in spec — JSON-based)

These were never built on the old branch. Greenfield.

---

## Decision log

Captures decisions already made so future work doesn't relitigate:

| Decision | Rationale |
|---|---|
| Skip painter classes (NotePainter, LanePainter, etc.) | Frame system in v1.2.3 supersedes their architectural goal. Bringing them back would re-introduce conflict and ~300 LOC of duplication. |
| Skip `NoteRenderContext` struct | Frame system + cached config in `NoteRenderer::populate` already covers per-frame state passing. |
| Keep curved image cache in `NoteRenderer` | Make `getCurvedImage` public for write-mode access. Don't move to a separate class. |
| Use REAPER native undo via `Undo_BeginBlock2`/`EndBlock2` | Already on branch; respects user's existing undo history. |
| `WriteController` as single state owner | Cleaner than scattering write state across PluginEditor + HighwayComponent + Renderers. Already structured this way on old branch. |
| `HitTestMapper` as standalone util | Pure pixel→music conversion, no DAW-specific logic. Reusable for future features (e.g., note-info tooltips). |
| Drag preview renders in `HighwayComponent::paintOverChildren` | Already has the right transform context. Don't introduce a new rendering layer. |
| `SustainRenderer` exposes single-sustain entry point | For drag preview tail. Replaces LanePainter from old branch. |

---

## Risk register

Sorted by where the unknowns are:

**HIGH** — needs design pass before implementation:
- Marquee perspective trapezoid — never built. Geometry is novel.
- Sustain drag polish — high LOC concentration, easy to miss subtle behavior

**MEDIUM** — known patterns but coordination required:
- Drag preview rendering integration with existing transforms
- Multi-ghost rendering layered on hover ghost
- Undo block scoping for paint stroke / erase sweep

**LOW** — mechanical:
- Mode toggle state machine
- Step grid + snap
- Single click / right-click placement
- Hover cursor (port of existing math)

---

## Open questions (from `WRITE_MODE.md` — bubble up before relevant phase)

- **Snap-off resolution** (Phase 1/2): 1/128th vs investigate community norm? Resolve before sustain drag polish.
- **Copy/paste cursor jump** (Phase 6): should cursor advance to end of pasted content?
- **Tap-rhythm during playback** (Phase 6+): possible exploration, no current commitment
- **"Select all in part"** (Phase 4 ext): how to scope `Cmd+A` — visible window, all in track, all in part?
- **2x kick exact key** (Phase 6): `K` is placeholder, may conflict with future bindings

---

## Old commit reference (quick-lookup)

For each phase, the old commits that contain the most relevant behavior. Read with `git show <SHA>`, don't cherry-pick.

| Feature | Old commits |
|---|---|
| WriteController structure | `7aa736a` |
| Mode toggle / keypress wiring | `aa2d8ac` |
| Click place / erase | `a9c8ecc` + `aa2d8ac` |
| Basic sustain drag | `37929ca` |
| Sustain cascade + snap | `30e76d4` |
| Sustain existing-note adjust | `afc31e0` |
| Sustain drag preview gem scale | `11d9d0e` |
| Cascade gap precision fix | `686cb8b` |
| Sustain body hit testing | `ceb4821` |
| Shift-paint + right-erase | `d9bd1cf` |
| Multi-ghost / X overlay | `59bee23` |
| Selection multi + shift-toggle | `aa2d8ac` + `66eabbc` |
| PPQ-based selection (scroll-stable) | `66eabbc` |
| Hover ghost (curvature, z, glow) | `f02f54d` |
| Notes past strikeline when paused | `d80e7ee` (already partially in v1.2.3) |
| HitTestMapper improvements | `0564f74` (extended range, clamping) |
| Lane simplification | `df90ee6` |

Workflow:
```sh
# Read what a commit did
git show <SHA>

# See file as it existed at that commit
git show <SHA>:Chartchotic/Source/Editor/WriteController.cpp

# Compare a file old-vs-new
git diff archive/old-wip-authoring..wip/authoring -- Chartchotic/Source/Editor/WriteController.cpp
```

---

## Why this isn't pre-emptive

You might read the per-phase implementation notes and think "this is over-planning." It's not, because:

1. We just got burned by cherry-picking without an architecture plan. The plan exists *because* the painter detour cost us a day.
2. Each phase has a concrete ship target. No phase is gated on "first do all phases" — Phase 1 ships as alpha, Phase 2 as beta, etc.
3. The risk register flags where unknowns are — that's where we'll iterate, not where the plan is rigid.
4. The decision log captures the *reasons* for choices so we don't relitigate (e.g., "why no painter?" has an answer).

---

## Next action

Start Phase 1: port `WriteController.{h,cpp}` from `archive/old-wip-authoring`, wire `W`/`Q` keypresses in `PluginEditor`, add mode pill expansion in `ToolbarComponent`, add STEP gridline type and writeMode opacity branch in `GridlineRenderer`. Build, verify mode pill works, ship as `1.3.0-alpha` if dev channel.
