<p align="center"><img src="docs/ampedup.png" width="112" alt="AMPEDUP! mountain and snowboard emblem"></p>

# AMPEDUP!

An unofficial static recompilation of the original Xbox version of *Amped: Freestyle Snowboarding* for Windows x64. It runs the original courses, physics, music and menus using data from your own disc.

[Build and install](docs/building.md) · [Preview notes](docs/preview.md) · [Credits](#credits)

## Play on Windows

The source build generates `Setup-AMPEDUP.exe` locally. Open it, choose your matching XISO and a new installation folder, then choose whether to create a desktop shortcut. Setup verifies the disc's SHA-256 before extracting it. Start `AMPEDUP!.exe` from the shortcut or installation folder.

Use your own European multilingual retail disc image. The supported XISO has SHA‑256 `1DCAF6F4C5385B90987A0DB33CBC34ED408982995DFE3A5B68DCFAD70CA91CB0`. Other editions and disc layouts are unverified. The repository contains the runtime, patch recipes and build tools. Bring your own disc to generate the executable and install game content.

## Features in the preview

- Vulkan rendering, native 16:9 or 4:3, and internal resolution from original through 4×.
- FXAA, SMAA, anisotropic filtering, and optional CRT scanlines.
- Xbox controller and keyboard support with tested Quick Start, Career saving and replay playback.
- A 60 FPS cap and an in-game settings menu. Open settings with **F1** or **View + Menu**. **Quit Game** is at the bottom.

Requirements are Windows 10 or 11 x64 and a Vulkan-capable GPU. Performance varies by course and render scale. [Preview notes](docs/preview.md) describe the tested paths and known gaps.

## Credits

The original game and its assets belong to their respective rights holders. AMPEDUP! is a fan project. The runtime and recompiler are based on [xboxrecomp](https://github.com/sp00nznet/xboxrecomp) and the Windows work in [Takedawn](https://github.com/xenophobentx/takedawn). Setup uses [extract-xiso](https://github.com/XboxDev/extract-xiso). See the vendored [licence](Amped-Host/toolkit/LICENSE) and [third-party notices](Amped-Host/toolkit/NOTICE) for component credits and terms.
