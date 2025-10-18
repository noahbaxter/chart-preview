# Roadmap & Implementation Phases

v0.9.0+ feature planning with effort estimates and phasing.

**Current Version**: v0.9.0 (Hit animations & REAPER integration)
**Last Updated**: 2025-10-18

---

## ✅ Recently Completed (v0.9.0)

- ✅ Hit animations (17 new animation frames)
- ✅ REAPER timeline integration (VST2/VST3)
- ✅ Modular pipeline architecture
- ✅ MIDI caching system
- ✅ Time-based rendering
- ✅ Gridline alignment fixes

---

## 🎯 Phase 2: UX Polish (v0.9.x → v0.10.0)

**Effort**: ~1 week

| Feature | Effort | Priority | Status |
|---------|--------|----------|--------|
| Default highway speed (1.15-1.20) | 5 min | P1 | Not started |
| Speed slider labeling | 30 min | P1 | Not started |
| Window sizing persistence | 1-2 hrs | P1 | Not started |
| REAPER mode toggle (click logo) | 2-3 hrs | P1 | Not started |
| Default HOPO 170 ticks | 1 hr | P1 | Not started |
| Make kicks wider | 1 hr | P1 | Not started |

---

## 🔥 Phase 3: Audio Features (v0.10.0+)

**Effort**: ~1 week each

| Feature | Effort | Priority | Status |
|---------|--------|----------|--------|
| Metronome click playback | 2-3 days | P2 | Not started |
| Guitar note click playback | 1 day | P2 | Not started |
| Drum sample playback | 3-4 days | P2 | Not started |
| Visual tempo/time sig markers | 1 day | P2 | Not started |

---

## 🔧 Phase 4: Advanced Integration (Future)

**Effort**: 3-5+ days each

| Feature | Effort | Priority | Status |
|---------|--------|----------|--------|
| Automatic REAPER track detection | 3-5 days | P2 | Research needed |
| Auto-scaling GUI | 1-2 days | P2 | Not started |
| Highway length control | 1-2 days | P2 | Not started |
| BRE support | TBD | P3 | Not started |
| Extended memory | 1-2 days | P3 | Not started |
| Real drums support | TBD | P3 | Planned |
| Pro Guitar/Bass support | 5+ days | P3 | Not started |

---

## 📋 Quick Wins (Pick from these!)

These are easy, high-impact improvements. Do these first:

1. **Default highway speed** - 5 min (many users want 1.15-1.20)
2. **Speed slider labeling** - 30 min (add "Slower ← → Faster" labels)
3. **Window sizing persistence** - 1-2 hrs (save/restore dimensions)

---

## 🚨 Critical Issues Waiting

See [USER_FEEDBACK.md](USER_FEEDBACK.md) for details:

- **Motion sickness from scrolling** (P0) - Investigation needed
- **Offset/sync compensation slider** (P0) - User timing adjustment

---

## 📚 Details & Discussion

For full feature descriptions, design considerations, technical notes, and user feedback:

→ See **[USER_FEEDBACK.md](USER_FEEDBACK.md)** (organized by user/priority)

---

## 🎮 Strategic Notes

- **Focus**: Be a great preview tool, not a Moonscraper replacement
- **Integration**: Point script requests to CAT (Clone Hero Assistant Tool) ecosystem
- **User Base**: Users mostly happy with core functionality, want polish + compatibility

---

**Legend:**
- P1 = Critical/high priority
- P2 = Medium priority (nice to have)
- P3 = Low priority (future/advanced)
- TBD = Effort estimate pending
