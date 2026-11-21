<p align="center"><img src="docs/ampedup.png" width="112" alt="AMPEDUP! mountain and snowboard emblem"></p>

# AMPEDUP! 0.7.1

An unofficial static recompilation of the original Xbox version of *Amped: Freestyle Snowboarding* for Windows x64. It runs the original courses, physics, music and menus using data from your own disc, with optional **21:9 ultrawide gameplay**.

<a href="https://recomplauncher.com/game/ampedup"><picture><source media="(prefers-color-scheme: dark)" srcset="https://recomplauncher.com/assets/press/launcher/badge-light.svg"><img alt="Get it on RecompLauncher" src="https://recomplauncher.com/assets/press/launcher/badge-dark.svg" height="48"></picture></a>

[Download for Windows](https://github.com/AMPEDUP-2001/AMPEDUP/releases/latest) · [System requirements](#system-requirements) · [Build from source](docs/building.md) · [Release notes](docs/release-0.7.1.md) · [Credits](#credits)

## Play on Windows

Choose a Windows installer from the [0.7.1 release](https://github.com/AMPEDUP-2001/AMPEDUP/releases/tag/v0.7.1).

- **Online** downloads the build tools during setup. [Setup-AMPEDUP-0.7.1-Online.exe](https://github.com/AMPEDUP-2001/AMPEDUP/releases/download/v0.7.1/Setup-AMPEDUP-0.7.1-Online.exe)
- **All-in-one** includes the build tools and installs offline. [Setup-AMPEDUP-0.7.1.exe](https://github.com/AMPEDUP-2001/AMPEDUP/releases/download/v0.7.1/Setup-AMPEDUP-0.7.1.exe)

1. Open the installer you downloaded.
2. Choose your matching XISO, a new installation folder, and whether to create a desktop shortcut.
3. Click **Install**. Setup checks the disc's SHA-256, extracts the game data and compiles the game on your PC. The build takes several minutes, plus download time with the online installer.
4. Start `AMPEDUP!.exe` from the shortcut or the installation folder.

Use your own European multilingual retail disc image. The supported XISO has SHA‑256 `1DCAF6F4C5385B90987A0DB33CBC34ED408982995DFE3A5B68DCFAD70CA91CB0`. Other editions and disc layouts are unverified. The repository contains the runtime, patch recipes and build tools. Bring your own disc to generate the executable and install game content.

For RecompLauncher, use the Windows x64 ZIP and select the matching XISO or its extracted folder in Easy Setup. Setup downloads the build tools and compiles the game locally. Play opens the game directly and settings, saves, and logs live under `profile/` in the ZIP's installation folder. See [launcher setup details](docs/building.md#package-for-recomplauncher).

## System requirements

- Windows 10 or 11 x64 and a Vulkan-capable GPU with a compatible driver.
- About 6 GB of free disk space for extraction and temporary build files.
- The matching retail XISO listed above. Internet access is required for the online installer.

**Test PC.** Windows 11 Pro, Intel Core i7-8700K (6 cores, 12 threads), NVIDIA GeForce RTX 3070 (8 GB VRAM), and 48 GB RAM. Minimum CPU, GPU, and RAM requirements are still unverified.

### Performance on the test PC

The development benchmark measured two 30-second gameplay samples per scale on **Snow Summit 1 / Miracle Mile**. It used Vulkan with VSync, a 1706 × 960 widescreen window on a 2560 × 1440 display at roughly 60 Hz, a 60 FPS cap, SMAA, 16× anisotropic filtering, CRT off, and muted audio.

| Internal scale | Avg. displayed FPS | Mean CPU draw | GPU busy (p95) | GPU budget left (p95) |
| --- | ---: | ---: | ---: | ---: |
| 1× | 56.22 | 7.43 ms (44.6%) | 8.65 ms (51.9%) | 48.1% |
| 2× | 58.15 | 6.56 ms (39.4%) | 8.84 ms (53.1%) | 46.9% |
| 3× | 58.52 | 6.48 ms (38.9%) | 9.18 ms (55.1%) | 44.9% |
| 4× | 58.50 | 6.37 ms (38.2%) | 9.56 ms (57.3%) | 42.7% |

The percentages use the 16.67 ms frame budget for 60 FPS. CPU draw measures rendering work on the CPU, and GPU busy measures active GPU time using PresentMon. CPU and GPU work can overlap. The slowest five-second window at 4× averaged 55.94 displayed FPS.

## Game features

- Vulkan rendering, 4:3 and native 16:9, and internal resolution from original through 4×.
- **21:9 ultrawide** widens the course camera and keeps the HUD and menus centered. Turn on **Options → Graphics → Ultrawide (21:9)** and confirm the restart. In RecompLauncher, choose **21:9** under **Aspect ratio** before launching.
- FXAA, SMAA, anisotropic filtering, and optional CRT scanlines.
- Xbox controller and keyboard support with tested Quick Start, Career saving and replay playback.
- Open **Options → Graphics** or press **F1** or **View + Menu**. The Audio tab has speaker mode, two mixes, and soundtrack, effects and master levels, and spatial width.
- **Quit Game** replaces **Game Demos** on the Main Menu. Settings that need a restart ask for confirmation.
- Optional **HD Texture Pack** adds detail to selected snow, pine bark, and metal. It starts Off.
- Miracle Mile has rounder rail and funbox details and restored snowboard powder. Brighton 1 / Hawkeye has targeted flicker fixes at Molly’s Lodge.

## Custom soundtracks

1. Open **Options → Soundtracks** and press **Y** on a controller or **Ctrl** on the keyboard.
2. Choose a folder containing MP3, FLAC, WAV, or Ogg Vorbis tracks. Each folder adds one genre.
3. Choose **Yes** to restart now or **No** to load the new genre the next time you start the game.
4. Open Soundtracks, press **X** to enable the new genre, then **A** to accept. Use **Save As Defaults** in Options to keep your selection.

Imports use the Windows Media components and save converted copies in your local profile. The import keeps existing genres, and short tracks below the game's encoded size minimum are skipped.

## Screenshots from 0.7.1

![Original Xbox and AMPEDUP! Main Menus, showing Quit Game in the sixth slot](docs/screenshots/0.7.1/main-menu-comparison.jpg)

![The native Graphics entry selected and the settings it opens](docs/screenshots/0.7.1/graphics-menu.jpg)

See the [release notes and comparisons](docs/release-0.7.1.md) for board powder, rails, HD textures, and the Brighton window fix.

## Credits

The original game and its assets belong to their respective rights holders. AMPEDUP! is a fan project. The runtime and recompiler are based on [xboxrecomp](https://github.com/sp00nznet/xboxrecomp) and the Windows work in [Takedawn](https://github.com/xenophobentx/takedawn). The bulk dispatcher adapts [WNBDEV's pushbuffer work](https://github.com/WNBDEV/xboxrecomp/commit/83357acac82d6a82f2aeefd2888f670a4fd4bc2b). Setup uses [extract-xiso](https://github.com/XboxDev/extract-xiso). See the vendored [licence](Amped-Host/toolkit/LICENSE) and [third-party notices](Amped-Host/toolkit/NOTICE) for component credits and terms.

Projects consulted or supplied for reference include [Burnout 3](https://github.com/sp00nznet/burnout3), [SSX3-recomp](https://github.com/koenhendriks/SSX3-recomp), [rePOP](https://github.com/FulGerNet/rePOP), [Finest Hour Remastered](https://github.com/crazyriddler/FinestHourRemastered), [RE-Amped2](https://github.com/Nico-D-eu/RE-Amped2), and [Amped 3 Revamped](https://github.com/minty-hippo/amped3revamped).

The toolkit documentation also draws on [the upstream toolkit fork](https://github.com/apexJCL/xboxrecomp), [N64Recomp](https://github.com/N64Recomp/N64Recomp), [XenonRecomp](https://github.com/hedge-dev/XenonRecomp), and [ps3recomp](https://github.com/sp00nznet/ps3recomp).
