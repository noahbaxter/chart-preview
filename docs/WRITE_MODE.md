# Write Mode Interaction Spec

`W` toggles write mode. `Q` toggles between Draw and Edit sub-modes.
Write mode remembers which sub-mode you were in last. Entering/exiting write mode
does not clear selection. Write mode can be entered during playback — interactions are
blocked until paused, but mode is ready when you stop.

Scroll always moves highway regardless of mode.

---

## UI

- **Mode pill**: expands from current "EDIT" pill to show "DRAW" or "EDIT" in distinct colors
- **Write mode sub-toolbar**: slides down from main toolbar when write mode is active.
  Contains step size stepper, snap toggle, triplet toggle, kick/open filter, mode toggle.
- **Grid lines**: reflect current step size. Measure lines always visible as anchors.

---

## Grid & Snap

- Grid lines reflect current step size (1/4, 1/8, 1/16, etc.)
- Measure lines are always visible regardless of step size
- `[` / `]` — halve / double step division
- `T` — cycle tuplet (off → triplet → quintuplet → septuplet → off)
- `S` — toggle grid snap (off = 1/128th minimum resolution, not true freehand)
- Step size and snap state are shared across both modes
- Step size also available as value stepper in write mode sub-toolbar

---

## Draw Mode

Primary creation tool. Left-click places, right-click erases.
No selection capability — switch to Edit mode for that.

### Mouse

| Input                  | Action                                                          |
|------------------------|-----------------------------------------------------------------|
| Left click (empty)     | Place short note at grid position                               |
| Left click (on note)   | No-op (use right-click to erase)                                |
| Left drag              | Place note with sustain (see Sustain Drag below)                |
| Shift + left drag      | Paint: short notes along path, one per time step (see below)    |
| Right click            | Erase note at position                                          |
| Right drag             | Erase sweep — removes any note the cursor touches               |

### Click vs Drag Disambiguation

Distance-only threshold (~3px). Note appears immediately on mouse-down. If cursor moves
beyond threshold, it becomes a drag (sustain or paint depending on modifier).

### Sustain Drag Behavior

- Note position (time) is set by the initial click Y position
- Lane follows cursor X position in real-time (updates as you drag)
- Dragging UP (forward in time) extends the sustain — end snaps to grid
- Dragging DOWN does nothing — note stays short, held visually
- Release confirms: final lane = cursor X at release, final length = cursor Y distance upward
- Minimum sustain length applies — if drag is too short, note stays short
- Diagonal drag = lane change + sustain (lane from X, length from upward Y distance)

### Shift-Drag Paint Behavior

- One note per time step maximum — no chords from a single paint stroke
- If you cross back into a time step where you already placed a note (during THIS gesture),
  the previous note is replaced with the new lane
- Pre-existing notes are NEVER affected by the paint stroke
- If a pre-existing note exists at the same lane+time, the paint stroke skips that cell
- If a pre-existing note exists at a different lane at the same time, the paint stroke
  places its note alongside (creates a chord) — no interference
- All painted notes are short (no sustains)
- Can paint both forward (up) and backward (down) from the start point
- The entire paint stroke is one undo action

### Visual Feedback

- **Normal**: ghost cursor shows note shape at cursor position (existing behavior)
- **Shift held**: ghost shows 2-3 progressively smaller/faded ghosts trailing down the
  highway, indicating multi-note paint mode
- **Right-click**: ghost shows X overlay indicating erase mode

### Keyboard Step Input (Phase 4)

| Key     | Action                                                        |
|---------|---------------------------------------------------------------|
| 1-5     | Place note in lane 1-5 at playhead position                  |
| 0       | Place open note at playhead position                          |

- Advance happens on release of ALL currently held number keys
- Hold multiple keys simultaneously = chord at current position
- Each advance (release) = one undo action
- Step cursor is at the playhead/strikeline position

---

## Edit Mode

Selection and manipulation. Double-click for occasional note creation.

### Mouse

| Input                     | Action                          |
|---------------------------|---------------------------------|
| Left click (on note)      | Select note                     |
| Left click (empty)        | Clear selection                 |
| Shift + left click        | Add/remove note from selection  |
| Left drag (from empty)    | Marquee select                  |
| Double click (empty)      | Place note                      |
| Double click (on note)    | Remove note                     |

### Marquee Visual

Perspective-matched trapezoid that follows the highway's convergence. The shape adjusts
its angle based on vertical position (wider near strikeline, narrower toward far end),
consistent with the highway's perspective rendering.

### Keyboard (with selection)

| Key               | Action                                   |
|-------------------|------------------------------------------|
| Up / Down         | Time-shift selected notes by step size   |
| Left / Right      | Lane-shift selected notes (stops at edges, no wrap) |
| Delete / Backspace| Delete selected notes                    |
| Cmd+C / Cmd+V    | Copy / paste (Ctrl on Windows/Linux)     |
| Cmd+A            | Select all visible notes                  |

### Move Behavior

- All selected notes shift atomically (green+red at same time → red+yellow, never
  two notes at red simultaneously during the shift)
- Each arrow press = one undo action (no coalescing)
- Moves commit immediately to MIDI — no preview state
- If moved notes land on pre-existing notes, the pre-existing notes are replaced
- Undo restores the pre-existing notes and moves selected notes back

### Copy/Paste Behavior (Phase 4)

- Paste at cursor position (not playhead — cursor and playhead are distinct)
- First note aligns to cursor (no leading empty space)
- Relative positions between notes are preserved
- Details TBD — deprioritized for initial implementation

---

## Kick/Open Lane Filter

- `O` toggles kick/open filter on/off
- When active: notes above the kick/open lane are faded (transparent, still visible
  but clearly deactivated)
- All interactions (place, erase, select, move) are constrained to the kick/open lane only
- Visual signifier in sub-toolbar indicates filter is active
- Works in both Draw and Edit modes

### 2x Kick (Phase 4)

- `K` in Edit mode: with kicks selected, alternates every other note to 2x kick
- Batch operation on current selection, one undo action
- Exact key TBD

---

## Undo

- Single click (place or erase) = one undo action
- Drag gesture (mouse down to mouse up) = one undo action
- Arrow key nudge: each press = one undo action (no coalescing)
- Keyboard step: each advance (key release) = one undo action
- Delete selected = one undo action
- Paste = one undo action
- Uses REAPER's native undo via `Undo_BeginBlock2` / `EndBlock2`

---

## Global Keybind Summary

| Key     | Action                          | Context     |
|---------|---------------------------------|-------------|
| W       | Toggle write mode               | Always      |
| Q       | Toggle Draw / Edit              | Write mode  |
| S       | Toggle grid snap                | Write mode  |
| T       | Cycle tuplet (off/3/5/7)                  | Write mode  |
| O       | Toggle kick/open filter         | Write mode  |
| [ / ]   | Halve / double step division    | Write mode  |
| 0-5     | Step input (place note)         | Draw mode   |
| K       | 2x kick batch                   | Edit mode   |

Cross-platform: Cmd (macOS) = Ctrl (Windows/Linux). JUCE handles this via ModifierKeys.

---

## Build Phases

### Phase 1 — Core Draw + Edit

Foundation. Single-note interactions only. No multi-select, no paint, no step input.

- Draw mode: left-click place, right-click erase, sustain drag
- Edit mode: click select, double-click place/erase, arrow time-shift, arrow lane-shift, delete
- Mode toggle (Q), write mode toggle (W)
- Mode pill with Draw/Edit distinction
- Ghost cursor updates (existing for Draw, selection highlight for Edit)
- Undo per action
- Grid lines reflect step size
- Step size stepper ([ / ] keys + sub-toolbar control)
- Snap toggle (S)

Depends on: WriteController refactor (done)

### Phase 2 — Multi-Note Draw

The MS killer feature.

- Shift-drag paint with one-per-row constraint
- Right-click drag erase sweep
- Multi-ghost visual (shift held)
- X visual (right-click)
- Triplet toggle (T)

Depends on: Phase 1

### Phase 3 — Multi-Select Edit

Group operations.

- Marquee select with perspective-matched trapezoid
- Shift+click to add/remove from selection
- Atomic group move (time + lane)
- Group delete
- Cmd+A select all visible

Depends on: Phase 1

### Phase 4 — Extended Features

Power user tools. Each can ship independently.

- Keyboard step input (0-5 with advance-on-release)
- Kick/open lane filter (O toggle + visual fade)
- 2x kick batch operation (K)
- Copy/paste
- Write mode sub-toolbar UI

Depends on: Phase 3 (for selection-based features)

### Phase 5 — Configuration

- Remappable keybinds
- JSON-based config (export/import/share)
- Keybind presets

Depends on: Phase 4 (all keybinds defined before making them configurable)

---

## Open Questions (deferred)

- Snap off resolution: 1/128th or investigate what charting community actually uses?
- Copy/paste: should cursor jump to end of pasted content?
- Tap-rhythm input during playback: possible future exploration
- "Select all in part" via sidebar or other mechanism
- 2x kick exact key and behavior details
