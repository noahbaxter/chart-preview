# User Feedback & Feature Requests

A simple checklist of user-reported issues and requests, organized by reporter.

---

## 🐛 **Invontor** (Primary Beta Tester)
Main contributor and QA, active testing on real charts

- [ ] **Motion sickness from scrolling** 🤮 (P0 - CRITICAL)
  - Multiple users affected (not isolated to one person)
  - Current scroll speed slider helps but not 100%
  - Possible causes: scroll smoothing, frame drops, animation smoothness
  - Status: Needs investigation

- [ ] **Window resizing not preserved** 💾 (P2)
  - Window size resets on REAPER restart/project reload
  - Should save/restore dimensions to plugin state

- [ ] **Speed slider direction backwards** ⚡ (P2)
  - Slider behavior feels counterintuitive
  - Could add labels ("Slower ← → Faster")

- [ ] **Default highway speed** ⚙️ (QUICK - 5 min)
  - Should default to 1.15 or 1.20 (for compatibility with old plugin)
  - Currently uses plugin's default value

- [ ] **Bottom corner elements pushed too low** 👇 (P2)
  - UI elements in bottom corners sit too low
  - Prevents highway from rendering to screen edges
  - Request: move bottom elements higher for better vertical space

- [ ] **Extended memory feature** (Future)
  - Notes should remain visible when transport stops (not just during playback)

---

## 💬 **Community Feedback** (Multiple Users)

- [ ] **GUI auto-scaling to fit screen size** ⚙️ (P2)
  - Plugin window doesn't automatically scale to available screen space
  - Need to manually resize on different displays
  - Request: auto-detect screen size, scale proportionally, preserve aspect ratio

- [ ] **Offset/sync compensation slider** 🎵 (P0 - HIGH)
  - Need manual audio sync offset compensation
  - Currently has latency sliders but users request direct offset control
  - Allow shifting chart timing relative to audio

- [ ] **Highway length control** 🛣️ (P2)
  - Users want configurable highway length/visible notes ahead
  - Currently hardcoded
  - Request: slider for how many beats ahead are visible

---

## 🎮 **Moonscraper Users Discussion** (Strategic)
Context: "am I just remaking moonscraper at this point?"

**Feedback**: Most Moonscraper users already use Moonscraper for charting, Chart Preview is for preview/visualization only
- Users: "my workflow is already to chart in moonscraper. i technically never need to look at the preview anyways"
- Only benefit switching: "60 FPS" (60 FPS preview)
- Most don't need editing capabilities in Chart Preview

**Strategic Recommendation**:
- Focus on being **great preview tool** not Moonscraper replacement
- Keep core features (preview, visualization, playback)
- Let Moonscraper handle editing workflows
- Differentiate on: real-time REAPER integration, audio playback, visual polish

---

## 📝 **Script/Tool Requests**

### .ini File Generation Script 📝 (P2)
**Reported By**: RAT KING, Dichotic
- Request: Generate .ini chart configuration files (Clone Hero format)
- Current: Dichotic has existing script in `folder_gen.py` (not user-friendly)
- Community suggestion: "all these scripts should 100% just be additions to CAT"
- **Recommendation**: Integrate with CAT (Clone Hero Assistant Tool) instead of bundling in VST

### Note Length Validation Script ✅ (QUICK)
**Reported By**: Google Sheets (user)
- Request: "Can we make a script for note lengths so I don't get yelled at by encore anymore"
- Validate sustain lengths against Clone Hero/Encore requirements
- Could be useful utility in REAPER via ReaScript

---

## 📊 **Priority Summary**

### 🚨 Critical (P0) - Block everything else
- [ ] Motion sickness investigation (research heavy)
- [ ] Offset/sync compensation slider

### 🔥 High (P1) - Next phase
- [ ] Default highway speed to 1.15-1.20 (5 min - QUICK WIN)
- [ ] Speed slider labeling (30 min)
- [ ] Window sizing persistence (1-2 hrs)
- [ ] Automatic REAPER track detection (3-5 days research)

### 📋 Medium (P2) - Polish
- [ ] GUI auto-scaling to screen size
- [ ] Highway length control
- [ ] Bottom UI elements pushed higher
- [ ] Speed slider direction clarity

### 📚 Low (P3) - Future
- [ ] Real drums support (strategic focus mentioned)
- [ ] Visual markers (solos/BREs/sections)
- [ ] Vocals support
- [ ] .ini generation (suggest CAT integration)
- [ ] Note length validation (suggest CAT or ReaScript)

### ✅ Already Complete (v0.9.0+)
- ✅ Hit animations (v0.9.0)
- ✅ REAPER integration (v0.8.7-v0.9.0)
- ✅ Time-based rendering
- ✅ Tempo/time sig marker infrastructure

---

## 🎯 **Quick Wins** (Easy to implement)

1. **Default highway speed** - 5 minutes
2. **Speed slider labeling** - 30 minutes
3. **Window sizing persistence** - 1-2 hours

These would address several user pain points with minimal effort.

---

**Last Updated**: 2025-10-18
**Legend**:
- 🤮 Motion sickness issue
- ✅ Complete
- [ ] To do
- P0-P3 = Priority level (0=critical, 3=nice to have)
