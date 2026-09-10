# Elite Drums test chart

Every feature in the ED spec, as MIDI notes and velocities, with what Chartchotic does with
each today. Track name must be exactly `PART ELITE_DRUMS`. Set `diff_elite_drums` in song.ini.

Load `_refs/midi/elite-drums.md` Appendix H into REAPER (Load note/CC names from file) to get
these as labels.

## Velocity semantics

Two different meanings depending on the note:

| Note | Velocity | Meaning |
| :--- | :------- | :------ |
| Hand gems, Kick | 1 | Ghost |
| Hand gems, Kick | 127 | Accent |
| Hand gems, Kick | anything else | Normal |
| Pedal Down (72) | 1 | No pedal gem, still terminates hi-hat sustains |
| Pedal Down (72) | 2-126 | Stomp |
| Pedal Down (72) | 127 | Splash |
| Roll lanes (110-118) | 41-50 | Applies to Hard as well as Expert |
| Roll lanes (110-118) | anything else | Expert only |

Chartchotic matches ghost and accent on the EXACT values 1 and 127. A chart using velocity 30
for ghosts reads as normal. Worth testing both ways if you care about tolerant parsing.

## Pitch reference, all four difficulties

| Gem | X | H | M | E |
| :-- | :- | :- | :- | :- |
| Pedal Down | 72 | 48 | 24 | 0 |
| 2x Kick | 73 | 49 | 25 | 1 |
| Kick | 74 | 50 | 26 | 2 |
| Snare | 75 | 51 | 27 | 3 |
| Hi-Hat | 76 | 52 | 28 | 4 |
| Left Crash | 77 | 53 | 29 | 5 |
| Tom 1 | 78 | 54 | 30 | 6 |
| Tom 2 | 79 | 55 | 31 | 7 |
| Tom 3 | 80 | 56 | 32 | 8 |
| Ride | 81 | 57 | 33 | 9 |
| Right Crash | 82 | 58 | 34 | 10 |
| Flam marker | 87 | 63 | 39 | 15 |
| Indifferent Yellow | 88 | 64 | 40 | 16 |
| Disco Flip marker | 90 | 66 | 42 | 18 |

Pan-difficulty: 103 Solo, 104 Overdrive/SP, 108 Stomp/Splash lane, 109 unused,
110 Kick lane, 111 Snare, 112 Hi-Hat, 113 L Crash, 114 Tom 1, 115 Tom 2, 116 Tom 3,
117 Ride, 118 R Crash, 120 Activation/BRE.

## Test sections

Expert pitches below. Repeat at -24/-48/-72 to test the other difficulties.

### 1. Every lane, plain

One note each, one per beat: 74, 75, 76, 77, 78, 79, 80, 81, 82. All velocity 100.
Checks lane order, colours, and drum-vs-cymbal models.

### 2. Dynamics on every lane

Three bars, all 8 hand lanes plus kick each bar:
- Bar A velocity 1 (ghost)
- Bar B velocity 100 (normal)
- Bar C velocity 127 (accent)

Kick ghost/accent is the part that was silently flattened until now, so chart 74 at
velocity 1 and 127 explicitly.

### 3. Kick and 2x kick

- 74 alone (1x)
- 73 alone (2x)
- 74 + 73 on the same tick (kick flam, Expert+)
- 74 + 73 on the same tick inside a kick roll lane (110), which per spec is treated as 1x
- 74 at velocity 1 and 127 (kick dynamics)

Test with the 2x kick toggle both on and off.

### 4. Hi-hat states

All on 76:
- 76 alone = Open (the default)
- 76 + 72 coincident = Closed
- 76 + 88 coincident = Indifferent
- 76 + 72 + 88 = Indifferent (the Indifferent marker wins)
- 72 dragged long under several 76 notes = all Closed
- Each of the above at velocity 1 and 127 for ghost/accent open and closed hats

### 5. Pedal gems (Stomp / Splash)

72 with no coincident 76:
- velocity 64 = Stomp
- velocity 127 = Splash
- velocity 1 = no gem, but terminates a running sustain

72 with a coincident 76, default: the Yellow suppresses the pedal gem, so you get a Closed
hat and no Stomp. Add `[STRICT_HAT_PEDAL_STATE]` as a text event at the start of the chart
and the same notes should give Closed + Stomp (velocity 2-126) and Open + Splash (127).

Indifferent (88) never suppresses the pedal gem, flag or no flag.

Appendix B of the spec is the exhaustive 20-row table. Worth charting all 20 rows if you want
full coverage.

### 6. Hi-hat sustains

- 76 (Open) notated shorter than 1/8, isolated: expect 1/4 sustain + 1/8 fadeout
- 76 short, with a 72 note-on within a dotted 1/4 after it: expect the sustain to extend to
  the 72 and terminate cleanly, no fadeout
- 76 notated 1/8 or longer: sustain matches the note length, last 1/8 is fadeout
- 76 notated exactly 1/8: fadeout only
- A run of 76 notes: should read as one continuous sustain, each terminating and renewing
- 76 followed within the sustain by 72, or by another 76, or by a Closed hat: clean termination
- A sustain totalling 1/16 or shorter: omitted entirely
- Splash (72 at velocity 127) generates a sustain the same way

### 7. Flams

- 87 over a single hand gem: that gem becomes a flam
- 87 over a ghost (velocity 1) and over an accent (velocity 127): flam keeps the dynamic
- 87 over an Open and a Closed hi-hat: flam keeps the hat state
- 87 over a gem inside a roll lane: ignored, draws as a single note
- 87 over a kick: no effect, kick flams are 74 + 73 instead
- 87 over two simultaneous hand gems: behaviour is undefined in the spec, so chart it and
  see what we do

### 8. Roll and tremolo lanes

One of each, with a run of notes underneath:
110 (Kick), 111 (Snare), 112 (Hi-Hat), 113 (L Crash), 114 (Tom 1), 115 (Tom 2),
116 (Tom 3), 117 (Ride), 118 (R Crash), 108 (Stomp/Splash).

- One lane at velocity 100 (Expert only)
- One lane at velocity 45 (Expert and Hard)
- Two or more lanes overlapping at the same time, which ED allows unlike RB3
- A flam (87) inside a lane, which should be ignored

Note 110, 111, 112 and 116 are the four that collide with guitar and 4-lane drum modifier
pitches. Chart all four, they were silently eaten until recently.

### 9. Star power, solo, BRE

- 104 over a phrase = Overdrive
- 103 over a phrase = Solo
- 120 over a phrase = Activation, plus a `[coda]` event on the EVENTS track to make it a BRE

### 10. Reserved notes, must be ignored

Chart one of each and confirm nothing renders and nothing breaks:
84 (inverted footing), 85 (LH sticking), 86 (RH sticking), 89 (sizzle hat),
91 (L bell/rim), 92 (R bell/rim), 93 (L choke), 94 (R choke),
105 (P1 Vs phrase), 106 (P2 Vs phrase).

### 11. Difficulty coverage

Repeat sections 1-4 at Hard (-24), Medium (-48) and Easy (-72). The 2x kick exists at every
difficulty in the pitch table, but is Expert-only in play.

### 12. Downchart-only markers

- 90 Disco Flip: per spec this has no effect on ED itself, only on the non-Pro 4L downchart.
  Chart it and confirm the ED highway is unchanged.
- `[snare_stem 1]` text event: downchart only, no ED effect.
- Channel flags 11-14 on hand gems: force the downcharted colour, no ED effect.

## What Chartchotic does with each today

| Feature | Status |
| :------ | :----- |
| Hand gems 75-82, kick 74, 2x kick 73 | Renders |
| Kick dynamics (74 at velocity 1 / 127) | Parses; ghost draws fainter, accent draws thicker with a centre line |
| Hand gem dynamics | Renders (ghost = short glyph, accent = chevron overlay) |
| Hi-hat Open / Closed / Indifferent | Renders |
| Star power 104 | Renders |
| Roll lanes 110-118 | Renders |
| Roll lane 108 (Stomp/Splash) | Ignored, not in the recognised range |
| Reserved notes | Ignored, as required |
| Disco flip 90 | Ignored, which is correct for ED |
| Flam 87 | Ignored, no parsing and no art |
| Kick flam (73 + 74) | Draws as two separate bars, not a flam |
| Stomp / Splash gems | Nothing. Columns 10/11 have no MIDI source |
| `[STRICT_HAT_PEDAL_STATE]` | Not parsed |
| Hi-hat sustains | Nothing. Drums never emit a sustain, only lanes |
| Solo 103, Activation/BRE 120 | Not parsed, for any instrument |
| `[snare_stem]`, channel flags | Not parsed, downchart only |

So sections 5, 6, 7 and the 108 lane are expected to render nothing today. They are the
useful ones to have charted in advance, since they are what the remaining work has to light up.
