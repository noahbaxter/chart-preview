# Note Collision Rules

No notes on the same pitch may overlap. Ever. When a note is moved or created,
these rules apply in order:

## 1. Head-on-head replacement

If the moved note's start lands exactly on another note's start (same pitch),
the pre-existing note is deleted. The moved note takes its place with its own
velocity, duration, and properties.

## 2. Predecessor sustain truncation

If the moved note lands inside an existing note's sustain body (the existing
note starts before the moved note), the existing note's sustain is truncated
to end at the moved note's start position.

## 3. Forward sustain clipping

If the moved note's sustain extends past the start of the next note ahead on
the same pitch, the moved note's sustain is clipped to end at that note's
start. The note ahead survives unchanged.

## 4. Chain extend (sustain draw tool)

When a sustain is drawn through multiple notes, each note's sustain extends
as far as possible without overlapping the next note's start.

## Visual preview during arrow key moves

Arrow key moves are visual-only until committed (deselect, click, drag start).
The preview must match what the committed result would look like:

- Predecessor sustains visually truncated at the preview note's position
- Preview note's sustain visually clipped to the first note ahead
- Head-on-head: stationary note hidden, preview note shown
- Notes ahead of the preview note remain visible (they survive the clip)
- Sustains truncated below minimum length are hidden

## What does NOT happen

- Notes ahead are never deleted by a moved note's sustain (the sustain is
  clipped instead)
- Notes are never partially merged or combined
- The moved note's start position never changes during resolution
