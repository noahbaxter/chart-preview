# Chart Preview
Chart Preview is an au/vst plugin for DAWs that visualizes midi notes similar to how they appear in rhythm games like Clone Hero and YARG. It's designed to make it clearer how midi note placement will translate in game. To use it, just throw the plugin in a midi track in your DAW of choice.

This plugin supports any Windows or MacOS based 64bit DAW. Happy charting!

![Preview Image](preview.jpg)

Big massive thank you Invontor for the lovely custom art assets.

**DISCLAIMER**

Chart Preview is in active development and currently only released in beta. At the moment drums are *(mostly)* feature complete, but guitar is not so don't expect things like sustained notes for the moment. Please feel free to leave bug reports and feature requests as part of testing this tool.

### Why is this better than RBN Preview?
Aside from being able to run on macs and outside of 32bit compatability layers, here's some unique features you may appreciate.
* Guitar
  * Open Notes
  * Tap Notes
* Drums
  * Ghosts and Accents
  * 2x Kick
* General
  * Visibility toggle for star power, dynamics, 2x kicks, and cymbals
  * Chart zoom from 400ms to 1.2s
  * 60 FPS

### Note Regarding Performance
You can make things easier on your cpu by reducing the framerate below 60FPS and/or reducing your chart zoom so that fewer notes are rendered at once. That said this plugin shouldn't be too taxing and I'm able to run 4 instances at once without any issue on a 2021 m1 pro macbook for reference.


### Currently In-Development Features
#### Guitar
* Sustain Notes
* Heuristic HOPOs (when notes are naturally close together)
* Open HOPOs
* Trill/Tremelo sections
#### Drums
* Lanes (cymbal swells and rolls)
#### Misc
* Extended memory (currently forgets everything except what was just played making it useless when playback is stopped)
* Measure and grid lines
* Draw call optimizations
* Latency tweaks
  * Compensate for other plugins with latency in a project
  * User toggle to reduce/increase based on chart zoom (do people want this?)

### Future Development
* Real Drums support

This codebase is distributed under an MIT license, but note that all art assets are exempt and distributed only for build purposes.