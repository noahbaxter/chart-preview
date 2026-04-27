# Write Mode Rebuild Plan

**Branch:** `wip/authoring`  
**Base:** `8dac3de`  
**Spec:** `docs/WRITE_MODE.md`  
**Scope of this doc:** implementation plan for note authoring in the current preview architecture

---

## Goal

Add a cohesive write mode to the preview tool without reviving the old painter stack and without fighting the current frame-based render pipeline.

The product target is a real authoring surface for REAPER users: direct manipulation of the chart as it appears in-game, with behavior strong enough to stand beside established chart editors rather than a limited helper overlay.

The plan needs to optimize for three things:

1. A clean state owner for authoring behavior
2. Integration with the code that actually exists on this branch
3. Incremental milestones that can ship and be validated without committing to every power feature up front

---

## Current Reality

This branch already has some useful foundation:

- `MidiWriter` / `ReaperMidiWriter` exist for host-side note mutation
- `HitTestMapper` exists for screen-to-lane/time mapping
- REAPER write calls have already been moved toward the correct threading model
- The render path is now frame-driven: `PluginEditor` builds frame data, `HighwayComponent` renders it, renderers populate draw calls

This branch does **not** yet have:

- A `WriteController`
- Highway-level mouse authoring flow
- A write-mode UI surface in the toolbar
- Step-grid generation beyond the existing measure/beat/half-beat model
- A settled selection model for edit mode

The old draft mixed valid behavioral goals with some assumptions that do not match the live code. Most importantly:

- Gridline state is produced in `FrameDataBuilder` via `GridlineGenerator`, not injected late in `SceneRenderer::paint()`
- `PluginEditor` currently owns top-level keyboard handling, while `HighwayComponent` is render-oriented and does not yet participate in note authoring input
- The toolbar has no existing write pill or sub-toolbar scaffold, so that UI work should be treated as new UI, not a small extension

---

## Guardrails

- No painter-class resurrection
- No source changes implied by this doc outside the eventual write-mode implementation
- Do not artificially de-scope core authoring behavior just to reach a smaller MVP
- Write mode assumes a single active writable highway: one part at one difficulty
- Behavior should follow `docs/WRITE_MODE.md` unless this plan explicitly calls out a refinement or staging decision
- The highway is an alternative editing surface to the piano roll, not a detached toy UI

---

## Foundation Already On Branch

| Component | Commit | Why it matters |
|---|---|---|
| REAPER write API helpers | `218dc32` | Host undo + insert/delete/move primitives exist |
| `MidiWriter` interface | `c8803be` | Authoring logic can stay DAW-agnostic |
| `ReaperMidiWriter` | `c8803be` | REAPER implementation already exists |
| `HitTestMapper` | `e81ea1f` | Screen-space interaction can map into lane/time space |
| GUI-thread undo handling | `71124cf` | Undo/write path is already moving in the right direction |
| Current frame/render architecture | current branch | Must be treated as the source of truth, not worked around |

---

## What This Plan Is Solving

The rebuild should answer these design questions before code starts moving:

1. Where does write-mode state live?
2. How do keyboard and mouse input reach that state owner?
3. How does authoring state influence frame generation and overlay rendering?
4. How do we target the correct writable part+difficulty cleanly?
5. What is the minimum viable authoring loop that feels coherent?

If those seams are clear, everything else becomes porting and iteration instead of architectural churn.

In this context, "minimum viable" does **not** mean stripped-down. It means the first cohesive authoring loop that already feels legitimate for real chart work.

---

## Recommended Architecture

### 1. `WriteController` is the single authoring state owner

`WriteController` should own:

- write mode active/inactive
- draw vs edit sub-mode
- snap, step division, tuplet
- active target slot
- hover state
- in-progress drag or stroke state
- selection state

It should **not** paint directly and should **not** know JUCE component layout details beyond data passed into it.

### 2. `PluginEditor` remains the top-level coordinator

`PluginEditor` should own:

- constructing the `WriteController`
- routing keybinds
- wiring all highway callbacks into the controller
- pushing authoring state into frame-building and UI surfaces

This fits the current architecture better than trying to make `HighwayComponent` independently smart.

### 3. `HighwayComponent` becomes the authoring interaction surface

`HighwayComponent` should handle:

- mouse enter/move/exit
- mouse down/drag/up
- repainting transient overlays in `paintOverChildren`

But it should stay thin: translate events into a normalized authoring request, then call `WriteController`.

The intended interaction model is direct manipulation:

- Draw mode is create/remove/sustain authoring
- Edit mode is selection/manipulation
- Both operate on the same visible target, same grid, and same lane/time logic

### 4. Frame generation owns the write-grid model

Write mode changes to gridlines should be integrated where gridlines are actually produced now:

- `FrameDataBuilder`
- `GridlineGenerator`
- `HighwayFrameData`

That means write-mode grid behavior should be part of frame construction, not a renderer-local hack.

### 5. Renderers remain passive

Renderers may need:

- a write-mode visual flag
- helper methods made public for ghost/preview drawing

But they should not own interaction state or mutate authoring behavior.

---

## Data Flow

```text
                  ┌──────────────────────────┐
                  │     PluginEditor         │
                  │  - owns WriteController  │
                  │  - routes keybinds       │
                  │  - wires highway events  │
                  │  - feeds authoring state │
                  │    into frame build      │
                  └────────────┬─────────────┘
                               │
        ┌──────────────────────┼──────────────────────┐
        │                      │                      │
        ▼                      ▼                      ▼
┌───────────────┐   ┌──────────────────┐   ┌─────────────────────┐
│  Toolbar      │   │ HighwayComponent │   │ FrameDataBuilder /  │
│ - reflects    │   │ - mouse events   │   │ GridlineGenerator   │
│   mode + grid │   │ - HitTestMapper  │   │ - emits step grid   │
│ - shows note- │   │ - paintOver-     │   │   when write-active │
│   type toggles│   │   Children for   │   │ - tuplet-aware      │
└───────┬───────┘   │   ghosts/preview │   └──────────┬──────────┘
        │           └────────┬─────────┘              │
        │                    │                        │
        └────────────┬───────┴────────┬───────────────┘
                     ▼                ▼
            ┌─────────────────────────────┐
            │      WriteController        │
            │  - mode, sub-mode           │
            │  - selection                │
            │  - step/snap/tuplet         │
            │  - note-type toggles        │
            │  - hover + drag/stroke st.  │
            │  - resolves visible target  │
            │  - calls MidiWriter         │
            └────────────┬────────────────┘
                         │
                         ▼
              ┌─────────────────────────┐
              │      MidiWriter         │
              │  (ReaperMidiWriter impl)│
              │  - main-thread only     │
              │  - undo block scoping   │
              └─────────────────────────┘
```

**Key invariant:** `WriteController` is the only owner of authoring state. Every other component is either feeding events in or reading state out for display.

---

## Scope Strategy

The spec in `docs/WRITE_MODE.md` is good as a destination, but implementation should be staged around a usable authoring loop instead of around isolated features.

Recommended milestone order:

1. Authoring shell and routing
2. Grid and UI feedback
3. Single-note draw interactions
4. Edit-mode selection and transforms
5. Stroke-based and keyboard power tools
6. Nice-to-have authoring UI and configuration

This differs from the older draft mainly by moving the event-routing shell ahead of cosmetic phase slicing.

---

## Milestone 0: Pre-Implementation Decisions

These should be treated as settled before writing the first implementation patch.

### A. Write target model

- Write mode is per plugin instance
- Authoring assumes one active writable highway only
- That highway represents one part at one difficulty
- Selection is scoped to that one visible target

The older multi-highway framing should be removed from the implementation plan unless the general preview architecture later reintroduces a clearly subordinate secondary display.

### B. Persisted vs transient state

| Property key | Type | Persisted? | Notes |
|---|---|---|---|
| `writeSubMode` | string `"draw"`/`"edit"` | YES | Remembered across sessions |
| `writeStepDivision` | int (1..64) | YES | User's working grid |
| `writeTuplet` | int (0/3/5/7) | YES | 0 = off |
| `writeSnap` | bool | YES | Snap to grid |
| `writeKickFilter` | bool | YES | Reserved for Phase 6, persist when added |
| `writeNoteType*` toggles | various | YES | Persist current authored variant per instrument |
| `writeMode` (active/inactive) | bool | NO | Always starts off — never auto-engage |
| `writeSelection` | n/a | NO | Live in `WriteController` only |
| Hover / drag / stroke state | n/a | NO | Transient interaction state |
| Active visible target | n/a | NO | Resolved each frame from current view |

`WriteController::init()` reads these on startup; setter methods write back. Property keys live in `Source/Utils/ChartTypes.h` or a dedicated `WriteStateKeys` header to avoid stringly-typed sprawl.

### C. Note identity for edit mode

Before edit mode is built, choose one of these strategies explicitly:

- stable note references captured from the current resolved window
- on-demand re-hit-test by musical position
- raw REAPER note index only for immediate operations

The old plan assumed `noteIndex` would be enough. That is risky for multi-step edit operations because indices can shift after mutations. The edit-mode plan should prefer a musical identity model first, then resolve to host note indices at commit time.

### D. Threading rule

All `MidiWriter` mutation entry points are **message-thread only**. This is a design invariant.

Enforcement:

- Every public method on `ReaperMidiWriter` opens with `JUCE_ASSERT_MESSAGE_THREAD;` (mirrors v1.2.3's `da1d115` discipline for REAPER read APIs in `ReaperMidiProvider`).
- Audio thread never enters `WriteController` or `MidiWriter`. The plan does not introduce any code path that crosses this boundary.
- Hover, drag, paint, erase, selection ops all originate from JUCE GUI events, which are already on the message thread.
- Async work (e.g. coalescing rapid edits) is not in scope for V1. If introduced later, marshal to message thread via `MessageManager::callAsync`.

### E. Interaction semantics already decided

The following product rules are now considered settled at a high level:

- The highway is a direct-manipulation editing surface that should stay in sync with REAPER
- Draw and Edit are both first-class; neither is a second-tier mode
- Sustain authoring is positive-only:
  - start is anchored at mouse-down
  - end follows the nearest valid snapped release location
  - the note cannot extend backward behind its start
  - lane switches live with cursor position
- Commit-time semantics dominate for create/move transforms:
  - drag/move previews are non-destructive
  - collisions resolve on mouse-up, not during preview
  - committed move results overwrite collided destination notes
- Multi-note sustain extension is required:
  - dragging across successive notes in a lane should turn each crossed note into a sustain ending at the start of the next note
- Editing should feel atomic:
  - multi-note moves commit once on release
  - undo should treat the gesture as one action
  - dragged notes remain selected after release
  - clicking empty space clears the current selection
- Right-drag erase should be destructive-as-you-go, but grouped as one undoable gesture

### F. Interaction matrix

This table is the source of truth for interaction behavior. If prose elsewhere conflicts with the table, the table wins and the prose should be corrected.

| Mode | Start condition | Input | Result while held/dragging | Commit on release | Notes |
|---|---|---|---|---|---|
| Draw | Empty cell | Left click | Show placed note immediately | Place short note | Uses current snap/step/tuplet rules |
| Draw | Empty cell | Left drag | Show note immediately, then sustain preview once drag is established | Place/resize sustain | Same sustain rules as dragging from an existing note, including chained sustain behavior when crossing successive notes |
| Draw | Existing note under cursor | Left click/hold, no drag | No change | No-op | Draw mode does not left-click delete |
| Draw | Existing note under cursor | Left drag | Begin sustain authoring immediately as drag starts | Update note-off time based on release position | Same sustain rules as dragging from empty space; lane can switch live during drag |
| Draw | Existing note run in lane | Left drag across successive notes | Show sustain-extension behavior across crossed notes | Each crossed note sustains to next note start | Batch note-off rewrite, one gesture |
| Draw | Any notes crossed by cursor | Right drag | Erase crossed notes immediately | One undoable erase sweep | Destructive-as-you-go for clarity |
| Edit | Note under cursor | Left click | Select/highlight note | Selection remains | No mutation yet |
| Edit | Empty area | Left click | Clear current selection | Selection cleared | Important for fast reset |
| Edit | Selected note(s) | Left drag | Begin move immediately when mouse moves; show non-destructive preview | Commit moved notes atomically | Moved notes remain selected after release |
| Edit | Unselected note | Left drag | Select that note immediately and begin move preview | Commit moved note atomically | Keeps edit mode direct and avoids select-then-drag friction |
| Edit | Empty area | Left drag | Show snapped trapezoid marquee aligned to highway geometry | Select notes inside the snapped lane/time region | Marquee begins and ends from cursor positions snapped onto the highway plane |
| Edit | Region right of highway | Left drag | Show bulk-select gesture for visible content | Select all visible selectable notes in current target | Temporary v1 affordance for quick select-all |
| Edit | Empty cell | Double click | Place note immediately | Immediate | Contextual create |
| Edit | Existing note | Double click | Delete note immediately | Immediate | Contextual delete |

If a row is missing here, treat it as undecided behavior — surface it before implementing the affected milestone.

---

## Milestone 1: Authoring Shell

**Purpose:** create the minimum scaffolding that makes write mode real in the app, even before note editing is complete.

### Deliverables

- `WriteController` introduced and wired into `PluginEditor`
- keybind routing for `W`, `Q`, `[`, `]`, `T`, `S`
- keybind routing for essential authoring toggles and edit actions
- active/inactive write mode state
- draw/edit sub-mode state
- single active writable target model
- passive state exposed for toolbar and highway overlays

### Integration points

- `PluginEditor` constructs and owns `WriteController`
- `PluginEditor::keyPressed()` routes write-mode shortcuts before falling through
- each `HighwayComponent` gets callback wiring for hover/click/drag events
- `PluginEditor` updates `WriteController` each frame with playback state and slot context

### Files

**Create:**
- `Chartchotic/Source/Editor/WriteController.{h,cpp}` — port API surface from `archive/old-wip-authoring:Chartchotic/Source/Editor/WriteController.{h,cpp}` and adapt to current `MidiWriter` + `HitTestMapper`. Strip painter-related calls; the controller itself was painter-clean.

**Modify:**
- `Chartchotic/Source/PluginEditor.{h,cpp}` — owns `WriteController`, adds `keyPressed` routing for W/Q/[/]/T/S, wires highway callbacks
- `Chartchotic/Source/Visual/HighwayComponent.{h,cpp}` — minimal hooks for hover/click/drag; passes through to controller
- `Chartchotic/Source/UI/ToolbarComponent.{h,cpp}` — first read of `WriteController` state for the mode pill (visual change comes in Milestone 2)

### Why this milestone comes first

Without the shell, later work gets forced into ad hoc callbacks and state duplication. This is the point where the branch either gets simpler or starts drifting again.

### Definition of done

- write mode can be toggled on/off
- draw/edit mode can be toggled while write mode is active
- playback gating behavior is defined and enforced
- the active writable target is observable and stable
- no actual note mutation is required yet

---

## Milestone 2: Grid + UI Feedback

**Purpose:** make write mode legible before adding complex authoring gestures.

### Deliverables

- write-mode visual affordance in the toolbar
- current sub-mode visible
- current step size reflected in rendered gridlines
- snap and tuplet state represented in controller state and visible UI
- essential note-type toggle state represented in controller state and visible UI

Grid controls likely need two distinct control groups:

- one for general subdivision up/down
- one for tuplet mode selection/cycling

They are related, but should not be conflated into a single control if that makes the working grid harder to read.

### Key implementation note

Gridline work belongs in the frame pipeline:

- extend the gridline model if needed for step lines
- feed current write-grid config into frame building
- keep measure lines always visible

The old draft treated this as mostly a renderer concern. It should instead be a frame-data concern with renderer support.

### UI staging recommendation

Do **not** force both of these into the first UI patch:

- mode pill
- full write sub-toolbar

Recommended order:

1. mode indicator first
2. sub-toolbar later, once the interaction model is stable

That reduces UI churn while authoring behavior is still moving.

### Files

**Modify:**
- `Chartchotic/Source/Utils/ChartTypes.h` — add `Gridline::STEP` enum value
- `Chartchotic/Source/Visual/Managers/GridlineGenerator.{h,cpp}` — emit STEP gridlines per current step/tuplet when write mode active
- `Chartchotic/Source/Editor/FrameDataBuilder.{h,cpp}` — pass write-grid config into gridline generation
- `Chartchotic/Source/Visual/Managers/AssetManager.{h,cpp}` — register STEP gridline asset (own image, not aliased to HALF_BEAT)
- `Chartchotic/Source/Visual/Renderers/GridlineRenderer.{h,cpp}` — `bool writeMode` member; opacity branch for write-mode gridlines (MEASURE 1.0 / BEAT 0.6 / HALF_BEAT 0.35 / STEP 0.25)
- `Chartchotic/Source/UI/ToolbarComponent.{h,cpp}` — mode pill expands to show DRAW/EDIT in distinct colors

### Definition of done

- step changes are visible on the highway
- measure anchors remain readable
- toolbar clearly indicates when write mode is active and which sub-mode is selected

---

## Milestone 3: Single-Note Draw Mode

**Purpose:** reach the first actually useful chart-authoring loop.

### Deliverables

- left-click place short note
- right-click erase note
- left-drag create sustain
- snap-aware placement
- minimum sustain handling
- one undo action per gesture
- hover ghost and drag preview sufficient to make placement trustworthy
- contextual delete-on-release for existing notes
- sustain extension from existing notes by drag
- multi-note sustain pass across successive notes in-lane
- right-drag erase sweep with immediate visual removal

### Recommended behavioral cuts

Treat these as required for the first draw-mode milestone:

- accurate note placement
- erase under cursor
- sustain drag commit
- lane follows cursor during drag
- drag threshold
- gesture-scoped undo
- delete only commits on release
- click existing can promote into sustain instead of delete
- drag across note runs can extend each note to the next note start
- right-drag erase should remove crossed notes immediately while still committing as one gesture

### Main risk

The hardest part here is not note insertion itself. It is making the preview and commit semantics line up so the placed note lands exactly where the overlay promised.

The second hardest part is preserving the intended sustain semantics when dragging across existing note runs: this is not just "make one long sustain," it is a batch note-off rewrite across crossed notes.

### Definition of done

- click-to-place feels deterministic
- erase behavior is obvious and consistent
- sustain drag is usable
- overlay placement matches committed note placement closely enough to trust
- drag-from-existing-note behavior cleanly disambiguates no-op vs sustain authoring

### Files

**Modify:**
- `Chartchotic/Source/Editor/WriteController.{h,cpp}` — `placeNote`, `eraseNote`, `commitSustain`, drag state machine, multi-note sustain extension across crossed notes
- `Chartchotic/Source/Visual/HighwayComponent.{h,cpp}` — mouse handlers in DRAW mode, drag preview painting in `paintOverChildren`, hover ghost geometry
- `Chartchotic/Source/Visual/Renderers/SustainRenderer.{h,cpp}` — public single-sustain entry point for drag preview tail (replaces the old `LanePainter::paint` call)
- `Chartchotic/Source/Visual/Renderers/NoteRenderer.{h,cpp}` — make `getCurvedImage(...)` public so ghost cursor matches real note curvature
- `Chartchotic/Source/Visual/Utils/HitTestMapper.{h,cpp}` — extend tolerances and below-strikeline placement (port behavior from `0564f74` + `df90ee6`)

**Reference (old commits, read don't cherry-pick):** `7aa736a`, `a9c8ecc`, `aa2d8ac`, `37929ca`, `30e76d4`, `afc31e0`, `11d9d0e`, `686cb8b`, `f02f54d`, `0564f74`, `df90ee6`.

---

## Milestone 4: Edit Mode Core

**Purpose:** make write mode a complete authoring mode, not just a note dropper.

### Deliverables

- click-to-select
- clear selection on empty click
- shift-click add/remove
- delete selection
- lane nudge via left/right arrow
- time nudge via up/down arrow using current grid size
- one undo action per keypress or delete action
- click-and-drag move preview with commit on release
- selected note highlight state clear enough to trust

### Recommended sequencing

Build edit mode in two passes:

1. single-selection plus move/delete
2. multi-selection and marquee

The old draft went straight to multi-select framing. That increases risk before note identity and replacement semantics are settled.

For drag initiation:

- clicking a selected note and moving the mouse should begin a move immediately
- movement still resolves against the current snap/grid rules
- on mouse-up, the moved notes remain selected

### Collision rule to settle up front

For move operations, the rule is:

- preview first
- commit on release
- committed result overwrites collided destination notes

The remaining detail to lock is edge behavior at lane bounds.

Keyboard nudges should follow the charting mental model:

- Up/Down moves selected notes forward/backward in time by the current grid step
- Left/Right moves selected notes across lanes
- lane movement stops at the edges

### Definition of done

- selection state is stable across simple edit actions
- lane/time nudges are predictable
- delete is one-step undoable
- drag-move preview is trustworthy and non-destructive until release

### Files

**Modify:**
- `Chartchotic/Source/Editor/WriteController.{h,cpp}` — selection set (initially single, then prepared for multi in Milestone 5), move/delete ops, lane/time nudge with grid step, drag-move state machine
- `Chartchotic/Source/Visual/HighwayComponent.{h,cpp}` — EDIT-mode mouse handlers, selection highlight rendering in `paintOverChildren`, drag-move preview rendering
- `Chartchotic/Source/PluginEditor.{h,cpp}` — arrow-key + Delete routing into `WriteController`

**Reference:** `aa2d8ac`, `66eabbc`.

---

## Milestone 5: Multi-Note Draw + Multi-Select Edit

**Purpose:** add the high-value accelerated authoring tools once the basic loop is trusted.

### Draw-side deliverables

- shift-drag paint stroke
- right-drag erase sweep
- gesture-level dedupe semantics
- one undo block per stroke

### Edit-side deliverables

- marquee selection
- multi-selection move/delete
- collision-safe atomic commit for group drag moves
- copy/paste

Copy/paste rule:

- pasted notes overwrite collided destination notes on commit
- this should match the move semantics so paste does not introduce a second collision model

### Important note

Marquee selection should operate in musical space using snapped lane/time bounds, while rendering as a polygonal trapezoid that meshes with the highway geometry.

Rules:

- the start and end points of the marquee come from cursor positions snapped onto the highway plane
- the visible selection shape should look like the true region on the highway, not a flat screen-space rectangle
- selection resolution should be based on the snapped lane/time region, not raw pixel containment
- this should feel precise and chart-aware, not like dragging a generic desktop lasso
- selection and marquee should only affect elements that are currently visible/selectable in the view
- if a chart element type is hidden via the view controls, it should not be selected
- for now, dragging in a dedicated region to the right of the highway may serve as a quick "select everything visible" gesture

Visible/selectable now means:

- the note is visible between the strikeline and the fade-out point in the current display
- notes that exist in the MIDI clip but are outside the currently visible authoring region are not selectable by marquee/direct selection gestures

This milestone is high priority. Marquee multi-select is more important than keyboard step entry or project-map work.

Paint/erase stroke rule:

- paint strokes are additive only
- erase strokes are subtractive only
- revisiting a cell during the same paint stroke should do nothing
- stroke behavior should not flip notes back and forth within one gesture

### Definition of done

- stroke tools materially speed up note entry
- selection tools materially speed up cleanup and reshaping
- undo grouping remains intuitive

### Files

**Modify:**
- `Chartchotic/Source/Editor/WriteController.{h,cpp}` — paint stroke + erase sweep state machines, marquee selection, group move/delete, copy/paste
- `Chartchotic/Source/Visual/HighwayComponent.{h,cpp}` — modifier key tracking, multi-ghost rendering, X-overlay erase cue, marquee rendering
- `Chartchotic/Source/Visual/Utils/MarqueeGeometry.{h,cpp}` (NEW, possibly) — perspective trapezoid math + point-in-polygon hit testing

**Reference:** `d9bd1cf`, `59bee23`, `aa2d8ac`, `66eabbc`.

---

## Behavior Tiers

To keep the implementation roadmap coherent, behavior should be thought of in three tiers.

### Tier 1: Core Authoring

Must exist for write mode to feel like a legitimate editing surface:

- mode toggle and core keybindings
- grid/snap/tuplet feedback
- note placement
- sustain drag, including chained sustain behavior
- right-click/right-drag erase
- single-note and multi-note selection
- drag move, arrow-key nudges, delete
- visibility-aware marquee selection
- essential note-type toggles with shortcuts

### Tier 2: Acceleration Features

Important for speed once the core loop is trusted:

- copy/paste
- shift-paint additive strokes
- right-of-highway bulk select
- keyboard step input
- quick workflows for alternating kick and 2x kick

### Tier 3: Extended Authoring Tooling

Important, but should not destabilize the core authoring model:

- project-map navigation strip
- phrase/event authoring
- trill/tremolo/roll/BRE tooling
- advanced note-type UI refinements
- remappable keybind presets/config export

---

## Milestone 6: Power Tools

These should stay explicitly deferred until the basic mouse authoring loop is stable:

- keyboard step input `0-5`
- kick/open lane filter
- 2x kick batch helper
- write-mode sub-toolbar controls
- remappable keybinds / config export
- project-map navigation strip

These are worth doing, but none of them should determine the architecture of milestones 1 through 5.

Clarification:

- core write-mode keybindings are **not** part of deferred power tooling
- what is deferred here is extended/remappable keybind configuration and additional non-essential shortcut families

---

## Cross-Cutting Design Notes

### Single-target support

The controller should always know:

- which part/difficulty is currently visible and writable
- how that visible target resolves to the writable REAPER track/take

The product assumption is one active writable target at a time.

### Instrument mapping

Lane-to-pitch mapping needs one authoritative implementation in the controller or a helper it owns. It should not be spread across event handlers.

This mapping must account for:

- guitar/bass five-lane + open handling
- drums lane variants
- difficulty context
- cymbal/tom encoding rules

**Cymbal vs tom encoding (decision):** matches `TrackResolver`'s read-side detection — drum notes on yellow/blue/green pitch with velocity ≥110 are interpreted as cymbals, lower velocities as toms. Write mode follows the same convention:

- placing a tom-style note: standard velocity (e.g. 100)
- placing a cymbal: same pitch, velocity 110+
- toggling cymbal/tom on a selected note: rewrite the velocity in place, no pitch change

This avoids a separate "cymbal flag" parallel to the existing read path.

**New-note default velocity:** 100 (tom / standard). Cymbal placement uses 110+. Velocity 0 is reserved for note-off.

This is one of the main product-critical areas still to design clearly. The authoring model needs to make it easy to intentionally place:

- standard lane notes
- open notes / kick-style bar notes
- future note-type variants without making the core interaction confusing

Open/kick authoring needs to be fast inside the same interaction model as normal notes. It should not feel like switching to a secondary tool just to place a basic playable note type.

### Note-type authoring model

V1 should explicitly support note-type selection instead of assuming every placed note is "plain."

Recommended model:

- keep lane/time placement as the primary interaction
- use a small mode/palette surface in the write toolbar for note-type variants
- the currently selected note-type variant applies to newly authored notes and relevant edits
- every selectable variant in the palette also needs a keyboard shortcut; the toolbar cannot be the only way to reach it

This is a better fit than burying note-type choice in hard-to-discover modifiers, especially for drums.

Important behavior rule:

- in draw mode, the current toggle state determines what gets created
- in edit mode, selecting an existing note should update the toggle state to reflect that note's authored properties
- changing the toggles while notes are selected should update those selected notes

These toggles are therefore both:

- authoring state for new notes
- editable properties for existing selected notes

Open/kick-like notes may also need special lane targeting behavior rather than only behaving like a generic variant toggle. For now, the plan assumes the current dedicated off-highway click zone can remain in place for v1, with modifier-key placement kept open as a likely future refinement.

#### Drum note-type needs for v1

Based on the local drum references, these are the obvious authoring variants:

- normal note
- accent modifier
- ghost modifier
- kick
- expert+ / 2x kick
- cymbal/tom distinction for applicable lanes

Notes:

- red is always a tom-style note; cymbal marking only applies to yellow/blue/green-style pro drum lanes
- 2x kick is a distinct authored state, not just visual styling
- accent and ghost are authored modifiers, not just display flags

Recommended v1 drum controls:

- hit type: `normal | accent | ghost`
- kick mode: `normal kick | 2x kick`
- surface mode for non-red drum lanes: `tom | cymbal`

Additional requirement:

- switching between `kick` and `2x kick` needs to be fast enough for repeated alternation in double-kick sections
- this strongly suggests dedicated shortcuts, and possibly a quick alternating workflow rather than forcing repeated toolbar changes

The exact UI can still evolve, but the plan should assume these authored states are real and selectable.

#### Guitar note-type needs for v1

Based on the local 5-fret MIDI references, these are the obvious authored variants:

- standard note
- open note
- tap note
- force HOPO
- force strum

Notes:

- notes are strum by default, with HOPO determined automatically unless forced
- tap and open markers are important enough to treat as authored note-type choices, not post-hoc oddities

Recommended v1 guitar controls:

- note type: `standard | open`
- articulation: `normal | tap`
- force state: `auto | force HOPO | force strum`

Open-note authoring requirement:

- open notes must be quick to place in the same editing flow as standard notes
- for now, open/kick notes may continue using a special click zone outside the highway
- if that special click zone remains, it needs to be explicit and visually legible
- modifier-key placement remains a strong candidate refinement and should not be ruled out by the v1 design

#### Not in first note-type pass

These are real chart-authoring concepts, but do not need to be in the first note-type selection surface:

- trill/tremolo lanes
- star power / overdrive phrase authoring
- BRE / freestyle authoring
- roll lanes and other phrase-like drum mechanics
- text/SysEx-heavy phrase editing beyond the core note variants

### Note-type shortcut requirement

All authored note-type toggles need keyboard access in addition to toolbar affordances.

That means the eventual design should include shortcut bindings for at least:

- drum hit type selection
- drum cymbal/tom toggle where applicable
- drum kick vs 2x kick selection
- guitar open-note selection
- guitar tap toggle
- guitar force HOPO / force strum selection

The toolbar should show and reflect current state, but should not be the exclusive control surface for switching authoring variants.

This is an essential feature, not deferred polish. Shortcut-driven switching between core authoring variants needs to exist from the beginning of the real authoring workflow.

### Playback behavior

The spec says write mode may be entered during playback but interactions are blocked until pause. That is the correct policy. The plan should preserve controller state during playback while rejecting commit actions.

### Visual overlays

Ghost cursor, drag previews, erase indicators, and selection visuals should all render in `HighwayComponent::paintOverChildren()`, but the geometry inputs should come from controller-readable state. That keeps paint deterministic and logic centralized.

Preview semantics matter:

- placement and move previews should clearly communicate pending result without committing early
- move previews should allow overlap/translucency during drag
- collision cleanup only happens on release
- right-drag erase should be destructive-as-you-go for clarity, but still wrapped as one undoable interaction

### Snap and precision policy

Snap-off resolution should floor at `1/128`.

Tuplet-aware placement still matters:

- triplet, quintuplet, septuplet, and related arbitrary divisions need to map to valid note timing the target game format can actually read
- the write-mode math should avoid creating unusably precise placements outside the intended authoring constraints
- this should be documented explicitly alongside the authoring rules so users understand the precision floor and tuplet behavior

### Lane-bound behavior recommendation

Recommended rule: clamp the entire move so no selected note can move off the edge of the highway.

Why:

- it is more predictable than partially dropping notes
- it preserves the relative shape of the selection
- it avoids a destructive surprise during what should feel like a reversible transform

If later testing proves this feels too restrictive, partial dropping can be reconsidered, but full-selection clamp is the safer default for edit mode.

### Project map

Add a future navigation strip to the plan:

- purpose: scroll/navigation overview of the whole song
- not required for first authoring implementation
- initial version should stay visually clean
- likely content:
  - current viewport marker
  - section/event markers
  - optional minimal density hints later

### Testing shape

Unit tests are most valuable for:

- controller state transitions
- snap and step math
- lane/pitch mapping helpers
- gesture dedupe behavior
- selection transform rules

Manual validation is still required for:

- overlay alignment
- drag feel
- visible-target write targeting
- undo grouping through REAPER
- visibility-gated selection behavior

---

## Risks

### High

- Edit-mode note identity model is underspecified
- Marquee behavior may be harder than the old draft assumed
- Sustains and overlap rules can get subtle quickly
- Target-to-track write targeting must be correct

### Medium

- Gridline expansion could sprawl if not kept inside frame generation
- Toolbar UI could churn if the full sub-toolbar is attempted too early
- Ghost/preview visuals may tempt logic duplication from renderers

### Low

- top-level keybind routing
- persisted controller settings
- basic write-mode toggling

---

## Recommended Validation Sequence

### After Milestone 1

- toggle write mode repeatedly
- switch between draw and edit
- verify state persistence rules
- verify playback blocks commits without clearing state

### After Milestone 2

- vary step divisions and tuplets
- confirm measure lines remain anchors
- confirm write UI reflects controller state

### After Milestone 3

- place notes across lanes and multiple step sizes
- erase repeatedly on dense passages
- drag sustains at several angles
- undo every gesture type
- verify drag-from-existing-note vs no-op behavior feels safe and obvious
- verify crossed note-runs become chained sustains correctly

### After Milestone 4

- select, nudge, and delete in dense passages
- test collision handling during moves
- confirm selection remains coherent after undo/redo

### After Milestone 5

- paint/erase strokes over mixed existing content
- marquee-select near perspective extremes
- test group drag clamping at both lane edges
- verify hidden element types are not selectable
- verify right-of-highway drag selects all visible notes only

---

## Open Questions To Resolve Before Coding Specific Milestones

### Before Milestone 1

- how exactly does the visible target resolve to the correct writable track/take?
- what controller API should highways call so event normalization stays consistent?

### Before Milestone 3

- exact visual treatment for drag preview: does the in-progress sustain render with the same gem image and curvature as committed notes, or a stylized translucent variant?
- right-drag erase visual: cursor cue (X overlay) plus per-note flash, or only cursor cue?

### Before Milestone 4

- what is the note identity model for edit operations?

### Before Milestone 5

- what should copy/paste align to when it lands later: cursor, viewport center, or original absolute timing?
- do we need direct chord-authoring beyond normal selection/copy workflows, or is that future-only?

---

## Shipping Strategy

Each milestone maps to a dev-channel build. `wip/authoring` merges into `dev` after passing manual validation; `dev` builds publish a `dev-latest` prerelease. Stable tag (`1.3.0`) is cut after Milestones 1-4 are ship-quality and the visible-target resolution is solid.

| Milestone | Dev channel tag | Ship target | Notes |
|---|---|---|---|
| 1: Authoring shell | `1.3.0-alpha.1` | dev only | Mode toggle works, no editing |
| 2: Grid + UI feedback | `1.3.0-alpha.2` | dev only | Mode pill + grid changes visible |
| 3: Single-note draw | `1.3.0-beta.1` | dev only | First useful authoring loop |
| 4: Edit mode core | `1.3.0-beta.2` | dev → main candidate | Selection, nudge, delete, drag-move |
| 5: Multi-note + multi-select | `1.3.0` | main, tagged release | Stroke + marquee tools |
| 6: Power tools | `1.3.x` patches | as ready | Step input, kick filter, 2x kick, copy/paste, sub-toolbar |

No milestone is gated on Milestone 6. If Milestones 1-4 land cleanly, `1.3.0` ships without paint stroke / marquee — those become `1.3.1` / `1.4.0`.

**Painter classes never come back.** If a future structural refactor wants painter-style separation, that's a separate post-1.3.0 PR after authoring is stable.

---

## Old Branch Reference (read, don't cherry-pick)

The painter-era `wip/authoring` is preserved at `archive/old-wip-authoring`. Twenty-five commits of relevant behavior. **Read with `git show`, do not cherry-pick** — the painter-refactor base means most commits will conflict against current architecture.

| Behavior | Old commit(s) |
|---|---|
| `WriteController` structure | `7aa736a` |
| Mode toggle / keypress wiring | `aa2d8ac` |
| Click place / erase | `a9c8ecc` + `aa2d8ac` |
| Basic sustain drag | `37929ca` |
| Sustain cascade + snap | `30e76d4` |
| Sustain existing-note adjust | `afc31e0` |
| Drag preview gem scale | `11d9d0e` |
| Cascade gap precision | `686cb8b` |
| Sustain body hit testing | `ceb4821` |
| Shift-paint + right-drag erase | `d9bd1cf` |
| Multi-ghost + X overlay visuals | `59bee23` |
| Selection multi + shift-toggle | `aa2d8ac` + `66eabbc` |
| PPQ-based selection (scroll-stable) | `66eabbc` |
| Hover ghost (curvature, z, glow) | `f02f54d` |
| Notes past strikeline when paused | `d80e7ee` (already in v1.2.3) |
| HitTestMapper improvements | `0564f74` (extended range, clamping) |
| Lane simplification | `df90ee6` |

Workflow:
```sh
git show <SHA>                                  # full diff
git show <SHA>:Source/Editor/WriteController.cpp  # file at that commit
git diff archive/old-wip-authoring..wip/authoring -- <path>  # old vs current
```

`docs/WRITE_MODE.md` (already on this branch) is the spec.

---

## Practical Implementation Order

If work starts now, the order should be:

1. Introduce `WriteController` and wire key routing in `PluginEditor`
2. Wire essential authoring shortcuts from the beginning, including note-type toggles and edit nudges
3. Wire `HighwayComponent` mouse event flow into controller callbacks
4. Add write-mode state reflection to toolbar and frame/grid generation
5. Implement draw-mode click place / right-erase / sustain drag / chained sustain extension
6. Add edit-mode selection, drag-move preview, nudge, and delete
7. Add marquee multi-select and then copy/paste
8. Add stroke tools, keyboard step entry, and project-map navigation after the main authoring loop is already solid

This is the most important correction to the old draft: build the authoring shell first, then the full trustworthy authoring loop, then acceleration features.

---

## Definition Of Success

The rebuild is successful if:

- write mode fits naturally into the current frame architecture
- controller ownership is clear and remains clear as features grow
- basic note entry and editing feel trustworthy
- the visible highway feels like a legitimate chart-authoring surface
- later features can be added without another architectural reset

---

## Immediate Next Step

Before any implementation patch, do a short design pass for these remaining concrete decisions:

1. visible-target to writable-track resolution
2. edit-mode note identity model
3. lane-bound clamp implementation for group moves
4. exact visual treatment for move preview translucency and overlap feedback

Once those are explicit, Milestone 1 can be implemented cleanly.
