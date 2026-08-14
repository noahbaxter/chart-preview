#!/usr/bin/env python3
"""
Validate an exported chart folder.

Checks the things that are silently wrong rather than loudly broken: a chart
with dynamics but no ENABLE_CHART_DYNAMICS plays flat, a song_length that
disagrees with the audio scrubs wrong, unclosed note-ons hang forever. All of
those load fine and look fine.

Conventions asserted here were derived from 29 shipping charts: official Rock
Band 4, hand-authored Clone Hero customs, and click-midi output.

    python3 tests/chart_validate.py "<chart folder>"

Exits non-zero if any check fails. Stdlib only, so it runs anywhere; the audio
duration check is skipped when ffprobe is unavailable.
"""

import os
import struct
import subprocess
import sys
from collections import Counter

TEXT_META = 0x01
TRACK_NAME_META = 0x03
END_OF_TRACK_META = 0x2F
TEMPO_META = 0x51
TIMESIG_META = 0x58

GHOST_VELOCITY = 1
ACCENT_VELOCITY = 127
DYNAMICS_EVENT = "ENABLE_CHART_DYNAMICS"

AUDIO_NAMES = ("song.ogg", "song.opus", "song.mp3", "song.wav")


class Chart:
    def __init__(self):
        self.ticks_per_beat = 0
        self.tracks = []          # list of dicts


def _vlq(buf, i):
    value = 0
    while True:
        byte = buf[i]
        i += 1
        value = (value << 7) | (byte & 0x7F)
        if not byte & 0x80:
            return value, i


def parse_midi(path):
    """Minimal standard-MIDI reader. Returns a Chart, raises on malformed input."""
    raw = open(path, "rb").read()
    if raw[:4] != b"MThd":
        raise ValueError("not a MIDI file (no MThd)")

    header_len = struct.unpack(">I", raw[4:8])[0]
    _fmt, _ntrks, division = struct.unpack(">HHH", raw[8:14])

    chart = Chart()
    chart.ticks_per_beat = division

    i = 8 + header_len
    while i < len(raw):
        if raw[i:i + 4] != b"MTrk":
            raise ValueError(f"expected MTrk at byte {i}")
        track_len = struct.unpack(">I", raw[i + 4:i + 8])[0]
        end = i + 8 + track_len
        j = i + 8

        track = {"name": None, "texts": [], "pitches": Counter(),
                 "velocities": Counter(), "note_ons": 0, "note_offs": 0,
                 "open": {}, "zero_length": 0, "tempos": 0, "timesigs": 0}
        tick = 0
        running = None

        while j < end:
            delta, j = _vlq(raw, j)
            tick += delta
            status = raw[j]
            if status & 0x80:
                running = status
                j += 1
            else:
                status = running
                if status is None:
                    raise ValueError("running status with no prior status byte")

            if status == 0xFF:
                meta = raw[j]
                j += 1
                length, j = _vlq(raw, j)
                data = raw[j:j + length]
                j += length
                if meta == TRACK_NAME_META:
                    track["name"] = data.decode("latin-1", "replace")
                elif meta == TEXT_META:
                    track["texts"].append((tick, data.decode("latin-1", "replace")))
                elif meta == TEMPO_META:
                    track["tempos"] += 1
                elif meta == TIMESIG_META:
                    track["timesigs"] += 1
            elif status in (0xF0, 0xF7):
                length, j = _vlq(raw, j)
                j += length
            else:
                kind = status & 0xF0
                if kind in (0x80, 0x90, 0xA0, 0xB0, 0xE0):
                    d1, d2 = raw[j], raw[j + 1]
                    j += 2
                    if kind == 0x90 and d2 > 0:
                        track["note_ons"] += 1
                        track["pitches"][d1] += 1
                        track["velocities"][d2] += 1
                        track["open"].setdefault(d1, []).append(tick)
                    elif kind == 0x80 or (kind == 0x90 and d2 == 0):
                        track["note_offs"] += 1
                        starts = track["open"].get(d1)
                        if starts:
                            if tick - starts.pop(0) <= 0:
                                track["zero_length"] += 1
                else:
                    j += 1

        chart.tracks.append(track)
        i = end

    return chart


def audio_duration_seconds(path):
    """None when ffprobe is missing or the file is unreadable."""
    try:
        out = subprocess.run(
            ["ffprobe", "-v", "error", "-show_entries", "format=duration",
             "-of", "csv=p=0", path],
            capture_output=True, text=True, timeout=30)
        return float(out.stdout.strip())
    except (FileNotFoundError, ValueError, subprocess.SubprocessError):
        return None


def read_ini(path):
    values = {}
    for line in open(path, encoding="utf-8", errors="replace"):
        line = line.strip()
        if not line or line.startswith(("[", "#", ";")) or "=" not in line:
            continue
        key, _, value = line.partition("=")
        values[key.strip().lower()] = value.strip()
    return values


class Report:
    def __init__(self):
        self.failures = []
        self.warnings = []
        self.notes = []

    def fail(self, msg):
        self.failures.append(msg)

    def warn(self, msg):
        self.warnings.append(msg)

    def note(self, msg):
        self.notes.append(msg)


def validate(folder, report):
    # --- required files -----------------------------------------------------
    midi_path = None
    for name in ("notes.mid", "notes.MID"):
        candidate = os.path.join(folder, name)
        if os.path.isfile(candidate):
            midi_path = candidate
            if name != "notes.mid":
                report.warn(f"{name}: lowercase 'notes.mid' is safer, "
                            "Linux and Android builds are case-sensitive")
            break
    if not midi_path:
        report.fail("no notes.mid")
        return

    audio_path = next((os.path.join(folder, n) for n in AUDIO_NAMES
                       if os.path.isfile(os.path.join(folder, n))), None)
    if not audio_path:
        report.fail(f"no song audio (looked for {', '.join(AUDIO_NAMES)})")

    ini_path = os.path.join(folder, "song.ini")
    if not os.path.isfile(ini_path):
        report.fail("no song.ini")

    for art in ("album", "background"):
        if not any(os.path.isfile(os.path.join(folder, art + ext))
                   for ext in (".png", ".jpg", ".jpeg")):
            report.warn(f"no {art} art")

    # --- midi ---------------------------------------------------------------
    try:
        chart = parse_midi(midi_path)
    except Exception as exc:
        report.fail(f"notes.mid failed to parse: {exc}")
        return

    report.note(f"ticks_per_beat {chart.ticks_per_beat}, "
                f"{len(chart.tracks)} tracks")

    named = [t for t in chart.tracks if t["name"]]
    report.note("tracks: " + ", ".join(t["name"] for t in named))

    for track in chart.tracks:
        name = track["name"] or "(unnamed)"

        unclosed = sum(len(v) for v in track["open"].values())
        if unclosed:
            report.fail(f"{name}: {unclosed} note-ons never closed")
        if track["zero_length"]:
            report.fail(f"{name}: {track['zero_length']} zero-length notes")
        if track["note_ons"] != track["note_offs"]:
            report.fail(f"{name}: {track['note_ons']} note-ons vs "
                        f"{track['note_offs']} note-offs")

        if name and "DRUM" in name.upper():
            if track["note_ons"] == 0:
                report.fail(f"{name}: no notes")

        if name == "EVENTS":
            sections = [t for _, t in track["texts"]
                        if t.startswith("[section") or t.startswith("[prc_")]
            if not sections:
                report.warn("EVENTS: no section markers")
            else:
                generic = sum(1 for s in sections if s.startswith("[section Section "))
                report.note(f"EVENTS: {len(sections)} sections")
                if generic == len(sections):
                    report.warn(
                        f"EVENTS: all {generic} sections are generic "
                        "'Section N'; practice mode will be a wall of numbers")

    # --- dynamics, chart-level ----------------------------------------------
    # The switch is chart-level, not per-track: reference charts carry a single
    # bare ENABLE_CHART_DYNAMICS on PART DRUMS and let it cover PART
    # ELITE_DRUMS too. Checking per-track would flag every one of them.
    drum_tracks = [t for t in chart.tracks if t["name"] and "DRUM" in t["name"].upper()]
    dynamic_count = sum(t["velocities"].get(GHOST_VELOCITY, 0)
                        + t["velocities"].get(ACCENT_VELOCITY, 0)
                        for t in drum_tracks)
    switches = [text for t in drum_tracks for _, text in t["texts"]
                if DYNAMICS_EVENT in text.upper()]

    if dynamic_count and not switches:
        report.fail(f"{dynamic_count} ghost/accent notes but no {DYNAMICS_EVENT} "
                    "on any drum track — the game ignores every one of them")
    elif dynamic_count:
        report.note(f"{dynamic_count} dynamic notes, switch present")
    elif switches:
        report.warn(f"{DYNAMICS_EVENT} present but nothing uses it")

    if any(s.strip().startswith("[") for s in switches):
        report.warn(f"{DYNAMICS_EVENT} is bracketed; every shipping chart "
                    "writes it bare")

    # --- ini vs audio -------------------------------------------------------
    if os.path.isfile(ini_path):
        ini = read_ini(ini_path)
        for key in ("name", "artist", "charter"):
            if not ini.get(key):
                report.fail(f"song.ini: missing {key}")

        declared_ms = ini.get("song_length")
        if audio_path and declared_ms and declared_ms.isdigit():
            actual = audio_duration_seconds(audio_path)
            if actual is None:
                report.note("audio duration unchecked (no ffprobe)")
            else:
                drift = abs(actual - int(declared_ms) / 1000.0)
                if drift > 1.0:
                    report.fail(f"song.ini song_length {declared_ms}ms disagrees "
                                f"with {os.path.basename(audio_path)} "
                                f"({actual:.2f}s) by {drift:.2f}s")
                else:
                    report.note(f"song_length matches audio ({actual:.2f}s)")


def main(argv):
    if len(argv) != 2:
        print(__doc__.strip())
        return 2

    folder = argv[1]
    if not os.path.isdir(folder):
        print(f"not a folder: {folder}")
        return 2

    report = Report()
    validate(folder, report)

    print(f"\n{os.path.basename(os.path.normpath(folder))}")
    for note in report.notes:
        print(f"  .  {note}")
    for warning in report.warnings:
        print(f"  !  {warning}")
    for failure in report.failures:
        print(f"  X  {failure}")

    if report.failures:
        print(f"\nFAIL — {len(report.failures)} problem(s)")
        return 1
    print(f"\nOK{f' — {len(report.warnings)} warning(s)' if report.warnings else ''}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
