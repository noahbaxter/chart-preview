# Chart Preview
Chart Preview is an au/vst plugin for DAWs that visualizes midi notes similar to how they appear in rhythm games like Clone Hero and YARG. It's designed to make it clearer how midi note placement will translate in game. To use it, just throw the plugin on a midi channel in your DAW of choice, but use it in Reaper to leverage the sdk for 0 latency scrubbing.

This plugin supports Windows, macOS, and Linux based 64bit DAWs. Happy charting!

![Preview Image](preview.jpg)

Big massive thank you Inventor211 for the lovely custom art assets.

#### **DISCLAIMER**

Chart Preview is in active development (v0.9.0 beta). Both drums and guitar are feature-complete with full sustain, lane support, and hit animations! REAPER integration provides direct timeline access. See [roadmap](docs/TODO.md) for current status and known issues.

[Consider supporting the project!](https://www.paypal.com/donate/?business=3P35P46JLEDJA&no_recurring=0&item_name=Support+the+ongoing+development+of+Chart+Preview.&currency_code=USD)

### Install

See [releases](https://github.com/noahbaxter/chart-preview/releases) to download the latest builds for your platform.

##### macOS
**VST3:** Place `ChartPreview.vst3` in `/Library/Audio/Plug-Ins/VST3`  
**AU:** Place `ChartPreview.component` in `/Library/Audio/Plug-Ins/Components`

##### Windows  
Place `ChartPreview.vst3` in your DAW's VST3 directory:
- **Most DAWs:** `C:\Program Files\Common Files\VST3`
- **Steinberg/Cubase:** `C:\Program Files (x86)\Steinberg\VstPlugins` 
- **REAPER:** Check Options > Preferences > Plug-ins > VST for custom paths

##### Linux
Place the `ChartPreview.vst3` bundle in:
- **User:** `~/.vst3/` 
- **System:** `/usr/local/lib/vst3/` or `/usr/lib/vst3/`
- **Custom:** Check your DAW's VST3 scan paths

**Note:** I recommend you close and reopen your DAW after installation to ensure a proper plugin rescan.

### Quick Start

1. **Load Plugin:** Add Chart Preview to a MIDI track in your DAW
2. **Select Instrument:** Choose Guitar or Drums from the plugin interface
3. **Pick Difficulty:** Set skill level (Easy, Medium, Hard, Expert)
4. **Configure Settings:** Se your midi track #, latency (on non-reaper DAWs), frame rate, and zoom as needed
5. **Play & Chart:** Hit play and enjoy!

### Why is this better than RBN Preview?
Chart Preview is an open source replacement for RBN Preview, built from the ground up for use on modern platforms and with some unique features you may appreciate.
* Guitar
  * **Open Notes**
  * **Tap Notes**
  * **Lanes/Tremolo**
* Drums  
  * **Ghosts and Accents**
  * **2x Kick Support**
  * **Lanes**
* General
  * **Cross-platform** - Proper 64bit Windows, macOS, and Linux support
  * **Advanced Visibility Toggles** - Star power, dynamics, 2x kicks, cymbal detection
  * **Flexible Note Speed**
  * **60FPS Efficient Rendering**

## Documentation

- **[🗺️ Detailed Roadmap](docs/UPCOMING_FEATURES.md)** - Implementation phases with effort estimates
- **[🔧 Build Instructions](docs/BUILDING.md)** - How to compile from source
- **[📚 Documentation Hub](docs/)** - Complete reference guide

### What's New in v0.9.0
- ✨ **Hit Animations** - Per-column hit indicators with kick bar flashes (17 new animation frames!)
- ✨ **REAPER Integration** - Direct timeline access via VST2/VST3 extensions, use reaper for zero latency playback and scrubbing
- ✨ **Time-Based Rendering** - No more "dynamic" highway *(man that thing sucked...)*
- ✨ **Improved Gridlines** - Better aligned with gem notes, with much better responsiveness for time signatures abd bpm changes

### Previous Features (v0.8.x)
- ✅ **Sustain Notes** - Guitar sustain rendering with accurate timing
- ✅ **Drum/Cymbal Lanes** - Tremolo rolls and cymbal swells
- ✅ **Guitar Tremolo** - Trill sections using same lane system
- ✅ **HOPO Modes** - Configurable auto-HOPO timing (16th, dot 16th, 170 tick, 8th, off)
- ✅ **Resizable Interface** - Window scaling with locked aspect ratio
- ✅ **Linux Support** - Full cross-platform CI/CD pipeline
- ✅ **Chord Detection** - 10-tick tolerance for accurate chord grouping
- ✅ **Performance** - PPQ-based timing system with latency compensation

See the [complete roadmap](docs/TODO.md) for detailed status and priority ranking.

## License

This codebase is distributed under an MIT license, but note that all art assets are exempt and distributed only for build purposes.
