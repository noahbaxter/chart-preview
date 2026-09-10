#!/usr/bin/env python3
"""Generate an Elite Drums test chart covering every feature in the ED spec.

Laid out one lane at a time: every variant of a lane in a row at 32nd spacing, then a bar
of silence before the next lane. Reasoning for each case is in docs/elite-drums-test-chart.md.

    .venv/bin/python scripts/make_elite_test_chart.py [out.mid]
"""

import sys
from mido import MidiFile, MidiTrack, Message, MetaMessage, bpm2tempo

PPQ = 480
BAR = PPQ * 4
STEP = PPQ // 4          # 16th note, the spacing inside a group
GROUP = PPQ              # one beat between groups
HIT = STEP // 2          # note length, short enough not to touch the next 16th
BPM = 100                # drop this if you want everything further apart on the highway

# Expert pitches. Lower difficulties are a flat -24 per step.
PEDAL, KICK2X, KICK, SNARE, HIHAT, LCRASH, TOM1, TOM2, TOM3, RIDE, RCRASH = range(72, 83)
FLAM, INDIFF, DISCO = 87, 88, 90
SOLO, SP, BRE = 103, 104, 120
LANE_STOMP = 108
LANE = {KICK: 110, SNARE: 111, HIHAT: 112, LCRASH: 113,
        TOM1: 114, TOM2: 115, TOM3: 116, RIDE: 117, RCRASH: 118}
RESERVED = [84, 85, 86, 89, 91, 92, 93, 94, 105, 106]

GHOST, NORM, ACCENT = 1, 100, 127
DIFF_OFFSET = {"X": 0, "H": -24, "M": -48, "E": -72}

events = []     # (tick, pitch, velocity, length)
markers = []    # (tick, text)
cursor = 0      # next free tick, always advanced to a bar boundary between blocks


def emit(tick, pitch, vel=NORM, length=HIT):
    events.append((tick, pitch, vel, length))


def block(label):
    """Start a labelled block on a fresh bar."""
    global cursor
    if cursor % BAR:
        cursor += BAR - (cursor % BAR)
    markers.append((cursor, "[%s]" % label))
    return cursor


def gap(bars=1):
    """Round up to a bar boundary, then leave `bars` empty bars.

    Lane rows pass 0, so a lane occupies exactly one bar and the barline is the separator.
    Blocks whose content already fills a bar (roll lanes, phrase markers) take the default
    so they don't run together."""
    global cursor
    if cursor % BAR:
        cursor += BAR - (cursor % BAR)
    cursor += BAR * bars


def run(label, groups, sp_from=None):
    """One row for a lane. `groups` is a list of groups, each a list of [(pitch, vel), ...]
    chords. Chords inside a group sit a 16th apart; groups start a beat apart.

    `sp_from` is the group index where the star-power phrase starts. Every gem has a second
    set of art under SP, so a lane is only fully covered when its variants appear both ways.
    The phrase runs from that group to the end of the row, which is why the SP groups come
    last: one contiguous 104 note, not two."""
    global cursor
    t = block(label)
    sp_start = None
    for gi, group in enumerate(groups):
        if sp_from is not None and gi == sp_from:
            sp_start = t
        for i, chord in enumerate(group):
            for pitch, vel in chord:
                emit(t + i * STEP, pitch, vel)
        t += GROUP
    if sp_start is not None:
        emit(sp_start, SP, NORM, t - sp_start)
    cursor = t
    gap(0)                            # one lane per bar; the barline is the separator


def dyn(pitch, extra=()):
    """Ghost, normal, accent; then as flams; then both again, to sit under an SP phrase."""
    base = [[(pitch, v)] + [(e, NORM) for e in extra] for v in (GHOST, NORM, ACCENT)]
    flam = [c + [(FLAM, NORM)] for c in base]
    return [base, flam, base, flam]


# --- Hand lanes, one row each ----------------------------------------------------------
# Beats 1-2 plain, beats 3-4 the same under a star-power phrase. 12 renderable variants
# per hand lane: ghost/normal/accent, times plain/flam, times normal/SP.
SP_HALF = 2
run("snare",             dyn(SNARE),                 SP_HALF)
run("hihat open",        dyn(HIHAT),                 SP_HALF)
run("hihat closed",      dyn(HIHAT, extra=[PEDAL]),  SP_HALF)
run("hihat indifferent", dyn(HIHAT, extra=[INDIFF]), SP_HALF)
run("left crash",  dyn(LCRASH), SP_HALF)
run("tom 1",       dyn(TOM1),   SP_HALF)
run("tom 2",       dyn(TOM2),   SP_HALF)
run("tom 3",       dyn(TOM3),   SP_HALF)
run("ride",        dyn(RIDE),   SP_HALF)
run("right crash", dyn(RCRASH), SP_HALF)

# --- Kicks ------------------------------------------------------------------------------
# Kicks get the same treatment: the white SP bar is separate art from the orange one.
def kick_row(chord_fn):
    trio = [chord_fn(v) for v in (GHOST, NORM, ACCENT)]
    return [trio, trio]

run("kick",      kick_row(lambda v: [(KICK, v)]), 1)
run("kick 2x",   kick_row(lambda v: [(KICK2X, v)]), 1)
run("kick flam", kick_row(lambda v: [(KICK, v), (KICK2X, v)]), 1)

# --- Pedal gems, default behaviour (everything before the strict flag) --------------------
run("pedal default",
    [[[(PEDAL, 64)],                                       # stomp
      [(PEDAL, 127)],                                      # splash
      [(PEDAL, 1)]],                                       # no gem, terminator only
     [[(PEDAL, 64), (HIHAT, NORM)],                        # suppressed: closed hat, no stomp
      [(PEDAL, 64), (HIHAT, NORM), (INDIFF, NORM)]]])      # indiff never suppresses

# --- Pedal gems under the strict flag ----------------------------------------------------
# The flag cannot be turned off once set, so it sits after every default-behaviour case.
markers.append((block("pedal strict"), "[STRICT_HAT_PEDAL_STATE]"))
run("pedal strict notes",
    [[[(PEDAL, 64), (HIHAT, NORM)],     # closed + stomp
      [(PEDAL, 127), (HIHAT, NORM)],    # open + splash
      [(PEDAL, 1), (HIHAT, NORM)]]])    # closed, suppressed by velocity 1

# --- Hi-hat sustains. These need real note lengths, so not 32nds -------------------------
t = block("hihat sustains")
emit(t, HIHAT, NORM, 60)                                  # short, isolated: 1/4 + 1/8 fade
emit(t + PPQ * 2, HIHAT, NORM, 60)                        # short ...
emit(t + PPQ * 3, PEDAL, 1, 10)                           # ... terminated within a dotted 1/4
emit(t + BAR, HIHAT, NORM, PPQ)                           # manual: 1/4
emit(t + BAR + PPQ * 2, HIHAT, NORM, PPQ // 2)            # manual: exactly 1/8, fadeout only
for i in range(5):                                        # a run: one continuous sustain
    emit(t + BAR * 2 + i * (PPQ // 2), HIHAT, NORM, 60)
emit(t + BAR * 3, HIHAT, NORM, PPQ // 4)                  # 1/16 total: omitted entirely
emit(t + BAR * 3 + PPQ * 2, PEDAL, 127)                   # splash generates one too
cursor = t + BAR * 4
gap()

# --- Roll lanes, one bar each -------------------------------------------------------------
def fill(t, spacing, fn):
    """Fill one bar from `t` at `spacing`, calling fn(index, tick)."""
    for i in range(BAR // spacing):
        fn(i, t + i * spacing)


for gem, lane in LANE.items():
    t = block("lane %d" % lane)
    emit(t, lane, NORM, BAR)
    fill(t, STEP, lambda i, tk, g=gem: emit(tk, g, NORM))
    cursor = t + BAR
    gap()

t = block("lane 108 stomp")
emit(t, LANE_STOMP, NORM, BAR)
fill(t, STEP * 2, lambda i, tk: emit(tk, PEDAL, 64))
cursor = t + BAR
gap()

t = block("lane hard promoted")     # velocity 41-50 applies to Hard as well as Expert
emit(t, LANE[SNARE], 45, BAR)
fill(t, STEP, lambda i, tk: emit(tk, SNARE, NORM))
cursor = t + BAR
gap()

t = block("lanes overlapping")      # ED allows arbitrary lane combinations, RB3 does not
emit(t, LANE[SNARE], NORM, BAR)
emit(t, LANE[RIDE], NORM, BAR)
fill(t, STEP, lambda i, tk: emit(tk, SNARE if i % 2 else RIDE, NORM))
cursor = t + BAR
gap()

t = block("flam in lane")           # a flam inside a lane is ignored
emit(t, LANE[SNARE], NORM, BAR)
fill(t, STEP * 2, lambda i, tk: (emit(tk, SNARE, NORM), emit(tk, FLAM, NORM)))
cursor = t + BAR
gap()

# --- Phrase markers -----------------------------------------------------------------------
for label, marker, gem in (("star power", SP, SNARE), ("solo", SOLO, TOM1), ("bre", BRE, RCRASH)):
    t = block(label)
    emit(t, marker, NORM, BAR)
    fill(t, PPQ, lambda i, tk, g=gem: emit(tk, g, NORM))
    cursor = t + BAR
    gap()

# --- Reserved notes, must be ignored -------------------------------------------------------
run("reserved", [[[(p, NORM)] for p in RESERVED[i:i + 3]] for i in range(0, len(RESERVED), 3)])

# --- Downchart-only markers ----------------------------------------------------------------
t = block("disco flip")             # no ED effect per spec
emit(t, DISCO, NORM, BAR)
fill(t, PPQ, lambda i, tk: emit(tk, SNARE, NORM))
cursor = t + BAR
gap()
markers.append((cursor, "[snare_stem 1]"))
gap()

# --- Lower difficulties ----------------------------------------------------------------------
for d in ("H", "M", "E"):
    off = DIFF_OFFSET[d]
    for name, pitch in (("kick", KICK), ("snare", SNARE), ("hihat", HIHAT),
                        ("lcrash", LCRASH), ("tom1", TOM1), ("tom2", TOM2),
                        ("tom3", TOM3), ("ride", RIDE), ("rcrash", RCRASH)):
        run("%s %s" % (d, name),
            [[[(pitch + off, v)] for v in (GHOST, NORM, ACCENT)]])
    run("%s kick 2x" % d, [[[(KICK2X + off, NORM)]]])


def build(path):
    mid = MidiFile(type=1, ticks_per_beat=PPQ)

    tempo = MidiTrack()
    tempo.append(MetaMessage("track_name", name="tempo", time=0))
    tempo.append(MetaMessage("set_tempo", tempo=bpm2tempo(BPM), time=0))
    tempo.append(MetaMessage("time_signature", numerator=4, denominator=4, time=0))
    mid.tracks.append(tempo)

    track = MidiTrack()
    track.append(MetaMessage("track_name", name="PART ELITE_DRUMS", time=0))

    # Absolute-time events, then delta-encode. Note-offs sort before note-ons at the same
    # tick so a re-struck pitch does not swallow its own note-on.
    abs_events = [(tick, 0, MetaMessage("text", text=txt)) for tick, txt in markers]
    for tick, pitch, vel, length in events:
        abs_events.append((tick + length, -1, Message("note_off", note=pitch, velocity=0)))
        abs_events.append((tick, 1, Message("note_on", note=pitch, velocity=vel)))
    abs_events.sort(key=lambda e: (e[0], e[1]))

    prev = 0
    for tick, _, msg in abs_events:
        msg.time = tick - prev
        prev = tick
        track.append(msg)
    track.append(MetaMessage("end_of_track", time=0))
    mid.tracks.append(track)

    mid.save(path)
    return mid


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "tests/charts/elite_drums_test.mid"
    m = build(out)
    notes = sum(1 for x in m.tracks[1] if x.type == "note_on")
    bars = max(t for t, _, _, _ in events) // BAR + 1
    print("wrote %s: %d notes, %d blocks, %d bars, %d BPM"
          % (out, notes, len(markers), bars, BPM))
