# Z Positioning Investigation — Postmortem

**Status:** Reverted. Approach proven non-viable in fake-3D rendering. **Resolved by a different approach** (Frame system, Apr 22–25 2026) — see `docs/RENDERING_REFACTOR.md`. This doc preserved as reference for why position-space Z lifts can't work in the current pipeline.

**Author:** Noah Baxter, with Claude Code (April 11–21, 2026)

**TL;DR:** Two attempts (Apr 11–12 and Apr 20–21) to make gem-on-bar floating consistent across depth in the perspective renderer. Both attempts hit the same fundamental wall: `createPerspectiveGlyphRect` uses **two independent 1/z curves** for X/Y/width vs sprite height, so a position-space lift cannot project to a constant fraction of sprite size at all depths. No amount of tuning fixes this — the math is a sum of two non-matching exponentials. The real fix is converting to a true 3D pipeline. Reverted to pre-rework state.

---

## What problem we were trying to solve

Gems (cymbals, toms) sitting *above* the kick bar plane should look like the gap stays a constant fraction of the gem's size at every depth — at the strikeline (large gem) the gap looks proportional, and at the vanishing point (tiny gem) the gap should *also* look proportional. Otherwise the gem appears to "drift onto" the bar at depth, breaking the visual.

Originally reported by Invontor on Discord (Apr 3–10), tracked in `.planning/1.2.3-hotfix.md` Task 1.

---

## Architectural root cause

`Chartchotic/Source/Visual/Geometry/PositionMath.cpp::createPerspectiveGlyphRect` runs **two different perspective formulas in parallel**:

```cpp
// Used for X position, Y position, AND fretboard width:
progress = (1 - depth) / (1 + depth * k1)
// where k1 is derived from PerspectiveParams::exponentialCurve (~0.65 → k1 ≈ 1.115)

// Used for sprite HEIGHT only:
perspectiveScale = scaleNear / (1 + depth * (scaleNear - 1))
// where scaleNear is derived from highwayDepth/playerDistance/perspectiveStrength (~2.4)
//   → effective k2 ≈ scaleNear - 1 = 1.4
```

These are both 1/z forms but with **different denominators**. They will never match.

### What this means in practice

A position-space gemZ offset projects to a pixel gap that scales with the **derivative** of the Y interpolation: `1/(1+d·k1)²`. The sprite *height* scales with `perspectiveScale`, which is `1/(1+d·k2)¹`. The ratio `gap_pixels / sprite_height` is therefore not constant across depth — it changes shape with d.

In a real pinhole-3D camera both would scale identically (same z, same projection matrix). Here, because perspective is a hand-tuned trapezoid with separate parameters for "shape" and "depth", the two curves drift apart.

### Why no compensation knob fixes it

Several attempts to add a third curve (a `foreshorten` factor) to bring sprite height into line with the gap rate. Mathematically you can choose `foreshorten(d) = gapRate(d) / widthRatio(d)` and force `gap/sprite = constant`. We did this. But:

1. Sprite **width** still follows `widthProgress` (the same curve as Y position, but as an absolute interp not a derivative). So sprite **aspect** distorts at depth (gems get ~1.8× wider relative to native at d=0.9).
2. Compensating width too means sprites no longer match their lane width. Now they don't fit their lanes.
3. There is no choice of curves that makes (gap, sprite W, sprite H) all proportional at every d *except* by collapsing all three onto a single 1/z denominator — which is what true 3D does.

You can pick any two of {consistent gap-to-sprite, preserved aspect, lane-fitting width}. You can't have all three in this geometry.

---

## What was committed before being reverted

Two commits on `dev` ahead of `main` (now on `experiment/z-position-rework` branch):

- **`9128b93` "fix zOff: use position-space offsets before projection"** (Apr 11–12)
  - Replaced post-projection pixel Z offsets with position-space ones added to `adjustedPosition` *before* the perspective system.
  - `InstrumentOffsets` fields converted from reference pixels to position units.
  - All renderers (Note, Animation, Gridline) updated.
  - Debug panel Z table changed to position-space units.
  - **Looked like a fix** — center-positioning became correct at all depths. But the *visible gap* (opaque bottom of cymbal to opaque top of bar) still collapsed at far distance because of the two-curve mismatch.

- **`49f71a0` "move arcOffset to position-space, extract PositionMath helpers"** (Apr 12)
  - Same treatment for the curvature arc.
  - Extracted helper functions into `PositionMath`.
  - Same architectural limit applies.

---

## What was experimented with on Apr 20–21 (this session)

All on `experiment/z-position-rework` (uncommitted at time of revert), now committed there:

1. **`NOTE_DEPTH_FORESHORTEN` formula rewrite** — new derivation `foreshorten = gapRate/widthRatio`. Mathematically forces `gap/sprite = constant` in Position mode. **Was numerically nearly identical to the old hack with current params** — coincidentally produces the same output, no visible change.

2. **LiftMode toggle: Position vs Pixel.** Two modes:
   - *Position*: gem position shifted in adjustedPosition by `gemZ`. Side effect: also shifts X (lane convergence) and width.
   - *Pixel*: gem rect translated in pixel-space by `-pixFrac * sprite_height`. No X side effects, but doesn't match perspective semantics.

3. **Master Z + additive pixel lift** — `gemZ` always applies, `pixFrac` adds on top in Pixel mode. Still couldn't get circles to line up with gridlines at all depths.

4. **Diagnostic crosshair overlay** — green outer circle at bar plane Y, red inner circle at gem rect center, sized linearly with MIDI position. Made it visually obvious that the math was doing what it claimed; the residual drift the user saw was a sum of (a) curvature offset, (b) asset internal padding, (c) the two-curve mismatch above.

5. **Debug panel cleanup** — hid most sections, kept Z table + Pixel sliders + Curvature + Sizes. Mode-aware Z table at one point, then reverted to always-position when master-Z model emerged.

6. **Mode toggle: BAR_FRETBOARD_FIT changed 1.15 → 1.0** at user request.

The session demonstrated, with a diagnostic overlay, that the math is doing exactly what it claims to do. The remaining "it still looks wrong" comes from architectural limits + asset internal positioning — neither of which can be solved by tuning a number.

---

## Things tried earlier (Apr 11–12) per `memory/project_zoff_investigation.md`

For completeness:

1. **Post-projection pixel nudge with various scaling formulas** — `zOff *= (curWidth / strikeWidth) * foreshorten`, `progress` only, `progress * foreshorten`, `progress * rawRatio`, `1/(1+dk)²`. All either over- or under-shrinkage.
2. **Position-space offset** (= what 9128b93 committed). Center alignment correct, visible gap still collapses at distance.
3. Realization: visible gap has two components — (a) position separation between gem and bar centers, (b) sprite transparent padding. (a) scales with position math, (b) scales with sprite size. They use different curves. No single offset captures both.

---

## Why true 3D would fix it

In a real pinhole projection:

```
screen_y(world_y, world_z) = focal * world_y / world_z
screen_height(world_height, world_z) = focal * world_height / world_z
```

Both are `1/world_z`. Identical denominator → identical scaling rate → ratio `gap/sprite` is constant by construction, not by tuning.

The current `createPerspectiveGlyphRect` is a piecewise hand-tuned trapezoid that approximates 3D for a single ground plane. It looks great for a flat highway, but adding any "above the plane" element exposes the two-curve mismatch.

A true 3D path would replace `createPerspectiveGlyphRect` with a proper view-projection matrix and 3D coordinates per element. Notes get a small `world_y` offset above the bar plane; both project consistently. This is a several-thousand-line refactor and likely needs JUCE OpenGL bindings or a dedicated rasterizer.

---

## Resolution (Apr 25, 2026) — Frame system

The drift problem **was solved without going true 3D**. The Frame system on `refactor/frame-rendering` shares one anchor + one scale across all sprites in a row (bar + every gem in the chord). Both `offsetY` and `height` multiply by the same `scale.y`, so the `gap_pixels / sprite_height` ratio is invariant with depth **by construction**, not by tuning.

This sidesteps the two-curve mismatch entirely: instead of trying to make two different perspective curves agree at every depth, the row composites in strike-reference space (where both gap and sprite size are well-defined as constants) and projects as a unit. The two curves still disagree, but they no longer matter — every sprite in a row scales by the same factor.

What this doc still teaches:
- *Why* a position-space gemZ offset alone can't fix the drift in a fake-3D pipeline
- The asset-padding gotcha (cymbal cone offset within its PNG) is real and unrelated to the math
- True 3D is still the right answer for any future feature that needs *individual* world-Y offsets between elements (e.g. an animated lift effect)

See `docs/RENDERING_REFACTOR.md` for the Frame system itself.

---

## Decision (Apr 21, 2026)

- **Revert dev to `44069d4`** (last commit before `9128b93`). Restores the pre-rework "sort of functional" pixel-space Z behavior. Keeps the `RenderTypeConfig` refactor (`ba7a4f7`, `55777d9`, `44069d4`) which is unrelated and useful.
- ~~**Preserve experimental work on `experiment/z-position-rework` branch.**~~ Branch deleted Apr 25, 2026 — superseded by the Frame system (see Resolution above). The diagnostic crosshair overlay was the only piece worth keeping; if a future true-3D rewrite needs it, recover from git reflog or recreate (it's ~30 lines).
- **Add backlog item** for the eventual 3D rewrite. Cross-reference this doc.

---

## How to *not* run in circles next time

If a future session picks this up, **read this doc first** before touching any Z-related code. The constraints are:

1. The two-curve mismatch is structural. You cannot fix it with a single offset value, a single "foreshorten" knob, or a mode toggle.
2. Position-space and pixel-space lifts each have a known failure mode. Neither is "right" — they're different tradeoffs.
3. Curvature (`NOTE_CURVATURE`) shifts gem position-space and contributes to the drift, separately from `gemZ`.
4. Asset internal padding (cymbal cone in upper portion of PNG, transparent padding around it) interacts with rect-center positioning in non-obvious ways. Use the crosshair diagnostic from the experimental branch to separate math drift from asset drift before touching code.
5. The crosshair diagnostic itself is the most useful thing to come out of these sessions. If you re-attempt, **port the crosshair overlay first** so you can see what the math is doing in real-time.

If you find yourself trying yet another formula for the foreshorten knob: **stop**. The answer is true 3D, not another scalar tweak.

---

## Files / commits / branches

- This doc: `docs/Z_POSITIONING.md`
- The fix that shipped: `docs/RENDERING_REFACTOR.md` (Frame system)
- Pre-rework state: `git checkout 44069d4`
- Memory: `~/.claude/projects/-Users-noahbaxter-Code-personal-charting-chart-preview/memory/project_zoff_investigation.md` (Apr 11–12 notes)
- Hotfix planning: `.planning/1.2.3-hotfix.md` (Task 1 — was the trigger for the rework)
- Affected source: `Chartchotic/Source/Visual/Geometry/PositionMath.cpp`, `Chartchotic/Source/Visual/Geometry/PositionConstants.h`, `Chartchotic/Source/Visual/Renderers/{NoteRenderer,AnimationRenderer,GridlineRenderer,SustainRenderer,SceneRenderer}.cpp`
