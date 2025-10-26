# Development Roadmap

Current version: **v0.9.4**

---

## v0.9.5 - Quick Wins
**Effort**: < 1 week

- [ ] Default highway speed (1.15-1.20)
- [ ] Speed slider labeling ("Slower ← → Faster")
- [ ] Window sizing persistence
- [ ] Latency offset UI cleanup

---

## v1.0.0 - Core Features + UI Overhaul
**Effort**: 2-3 weeks | **Status**: Major release

### Chart Features
- [ ] Time sig changes display (symbols on left, scroll with highway)
- [ ] Section borders (EVENTS track parsing)
  - Blue measure lines for new sections
  - Section name text above highway (temporary overlay)
  - Future: section list/minimap on left (TBD design)
- [ ] Drum fills/BRE (full lanes for kicks/open)
  - Activation gem logic: Gcym > Bcym > Ycym > G > B > Y > R > Kick
- [ ] Solo sections (blue highway background during solo note)
- [ ] Better mouse scrolling (shift=faster, ctrl=precise)

### UI Overhaul
- [ ] Info display: BPM, time sig, measure, beat position
- [ ] Better menu system + advanced settings menu
- [ ] Settings persistence audit (all options saved/restored)
- [ ] Visual polish (doesn't look like dogshit)

---

## v1.1.0 - Real Drums Support
**Effort**: 2-3 weeks | **Phase**: Major feature

**Start with top hat mode** (minimal custom assets)

- [ ] Flam note types (split gems for 2 stick hits)
- [ ] HH open/closed/foot gem types
- [ ] Generic gem system (color any part of note)
- [ ] Reorderable lanes + custom colors
- [ ] Refactored MIDI parsing (supports reductions, benefits all instruments)

**Iterate after top hat:**
- [ ] 5-lane drums (RB drums + extra HH lane)
- [ ] 8-lane drums (every hit gets own lane)

---

## v1.2.0+ - Polish & Extras
**Effort**: 1-2 weeks each

- [ ] Note color customization (CH color profile templates)
- [ ] GH style gems toggle
- [ ] Instrument autodetection (by track name)
  - PART DRUMS, PART BASS, PART GUITAR, PART KEYS, PART VOCALS, etc.
  - Smart UI setup based on detected instrument
- [ ] GUI auto-scaling refinements
- [ ] Highway length control (configurable visible beats)
- [ ] Performance optimization (minimize expensive REAPER API calls)

---

## v2.0.0 - Authoring & Vocals
**Effort**: TBD | **Phase**: Long-term

### Authoring Features
- [ ] Moonscraper-style note placement (mouse + grid snapping)
- [ ] Snapping divisions (4th-64th notes, tuple support)
- [ ] Note eraser tool
- [ ] INI generation/export

### Vocals
- [ ] 2D pitch-based karaoke display
- [ ] Lyrics with rhythm timing

---

## Key Dependencies

- **EVENTS track parsing** (v1.0.0) → enables section detection + future autodetection
- **Real Drums MIDI refactor** (v1.1.0) → foundation for generic gem system + custom instruments
- **Instrument autodetection** (v1.2.0) → depends on EVENTS track already working

---

## Quick Reference

**Next immediate task**: v0.9.5 quick wins
**Most exciting work**: v1.1.0 Real Drums (start after v1.0.0 complete)
**See also**: USER_FEEDBACK.md (user requests), UPCOMING_FEATURES.md (phase history)
