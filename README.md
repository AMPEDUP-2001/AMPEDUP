<p align="center"><img src="docs/ampedup.png" width="112" alt="AMPEDUP! mountain and snowboard emblem"></p>

# AMPEDUP! 0.5

An unofficial static recompilation of the original Xbox version of *Amped: Freestyle Snowboarding* for Windows x64. It runs the original courses, physics, music and menus using data from your own disc.

[Download for Windows](https://github.com/AMPEDUP-2001/AMPEDUP/releases/latest) · [Build from source](docs/building.md) · [Release notes](docs/preview.md) · [Credits](#credits)

## Play on Windows

Choose a Windows installer from the [0.5 release](https://github.com/AMPEDUP-2001/AMPEDUP/releases/tag/v0.5).

- **Online** downloads the build tools during setup. [Setup-AMPEDUP-0.5-Online.exe](https://github.com/AMPEDUP-2001/AMPEDUP/releases/download/v0.5/Setup-AMPEDUP-0.5-Online.exe)
- **All-in-one** includes the build tools and installs offline. [Setup-AMPEDUP-0.5.exe](https://github.com/AMPEDUP-2001/AMPEDUP/releases/download/v0.5/Setup-AMPEDUP-0.5.exe)

1. Open the installer you downloaded.
2. Choose your matching XISO, a new installation folder, and whether to create a desktop shortcut.
3. Click **Install**. Setup checks the disc's SHA-256, extracts the game data and compiles the game on your PC. The build takes several minutes, plus download time with the online installer.
4. Start `AMPEDUP!.exe` from the shortcut or the installation folder.

Use your own European multilingual retail disc image. The supported XISO has SHA‑256 `1DCAF6F4C5385B90987A0DB33CBC34ED408982995DFE3A5B68DCFAD70CA91CB0`. Other editions and disc layouts are unverified. The repository contains the runtime, patch recipes and build tools. Bring your own disc to generate the executable and install game content.

## Features in the preview

- Vulkan rendering, native 16:9 or 4:3, and internal resolution from original through 4×.
- FXAA, SMAA, anisotropic filtering, and optional CRT scanlines.
- Xbox controller and keyboard support with tested Quick Start, Career saving and replay playback.
- A 60 FPS cap and an in-game settings menu. Open settings with **F1** or **View + Menu**. **Quit Game** is at the bottom.

Requirements are Windows 10 or 11 x64, a Vulkan-capable GPU, and about 6 GB of free space for extraction and temporary build files. Performance varies by course and render scale. [Release notes](docs/preview.md) describe the tested paths.

## Credits

The original game and its assets belong to their respective rights holders. AMPEDUP! is a fan project. The runtime and recompiler are based on [xboxrecomp](https://github.com/sp00nznet/xboxrecomp) and the Windows work in [Takedawn](https://github.com/xenophobentx/takedawn). Setup uses [extract-xiso](https://github.com/XboxDev/extract-xiso). See the vendored [licence](Amped-Host/toolkit/LICENSE) and [third-party notices](Amped-Host/toolkit/NOTICE) for component credits and terms.

Projects consulted or supplied for reference include [Burnout 3](https://github.com/sp00nznet/burnout3), [SSX3-recomp](https://github.com/koenhendriks/SSX3-recomp), [rePOP](https://github.com/FulGerNet/rePOP), [Finest Hour Remastered](https://github.com/crazyriddler/FinestHourRemastered), [RE-Amped2](https://github.com/Nico-D-eu/RE-Amped2), and [Amped 3 Revamped](https://github.com/minty-hippo/amped3revamped).

The toolkit documentation also draws on [the upstream toolkit fork](https://github.com/apexJCL/xboxrecomp), [N64Recomp](https://github.com/N64Recomp/N64Recomp), [XenonRecomp](https://github.com/hedge-dev/XenonRecomp), and [ps3recomp](https://github.com/sp00nznet/ps3recomp).
