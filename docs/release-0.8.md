# AMPEDUP! 0.8 · The optional HD Pack

Switch between **Original** and **HD Pack** in **Options → Graphics → Course textures**, then confirm the restart. Original is the default. The HD Pack adds surface detail close to the rider while keeping the colors, track marks and distant textures from your disc.

## What you’ll see

- **Snow.** Rough powder shared by all 11 courses gains wind-worked grain. Look at the edge of the groomed slope on Altibahn or the packed sidewall of the quarterpipe at Gunny’s Gulch.
- **Wood, rock and foliage.** Red-painted funboxes show the grain under their paint, the canyon walls at Gunny’s Gulch pick up sandstone grain, and nearby pine boughs on Snow Summit show finer needles.
- **Stratton village.** As you ride past the lodge windows, the siding now shows fine horizontal grain. The darker plank joints still come from the disc.
- **Course hardware.** Brighton’s blue safety fence and the Nixon JibFest finish net have visible weave. The green lift steel keeps its original rust and paint wear.
- **Board spray.** Fresh powder lifts from the edge of the snowboard during a turn. The effect follows the game’s existing particles. The track, collision and scoring still come from the original game.

![Gunny’s Gulch sandstone, Original and HD Pack](screenshots/hd-pack/gunnys-gulch-sandstone-sunlit.png)

![Snow Summit 2 painted funbox, Original and HD Pack](screenshots/hd-pack/snow-summit-painted-funbox.png)

![Village lodge siding in a matched Stratton replay](screenshots/hd-pack/stratton-lodge-siding.jpg)

[Open the comparison gallery](screenshots/hd-pack/README.md) for the full-size views, detail crops and a silent Brighton lift clip.

The [21:9 Snow Summit comparison](screenshots/hd-pack/snow-summit-ultrawide-pines.jpg) shows nearby pine branches from the same landing in both modes. An installed Windows build also loaded all 11 unlocked courses, recorded a short ride on each and returned to the menus with the HD Pack enabled at 4×.

## Installing or updating

Choose the **Online** Windows installer to download the build tools during setup, or **All-in-one** to install offline. Both verify the 610 game files from your own supported European multilingual XISO. A repacked XISO is accepted when its files match. RecompLauncher also accepts the extracted game folder. The in-game languages are English, Deutsch, Français, Italiano and Español.

RecompLauncher rebuilds the runtime for this release. Your profile holds your settings, career saves, replays and imported music separately from the compiled game. If a launcher update was interrupted while swapping runtimes, setup can recover a single identifiable previous installation on retry.

Building the HD textures during setup now uses less memory. In four Windows test builds, the texture generator’s peak working set fell from about 254 MiB to 156 MiB, with identical textures and rail geometry.

The offline installer, online installer and RecompLauncher package produced the same game executable from the verified disc files on the test PC. Setup now omits its temporary source path and build timestamp from that executable.

On the upgraded test PC, two 18-second fullscreen Snow Summit 2 samples with the 4× HD Pack averaged 59.92 and 59.94 displayed FPS. Neither had a display interval over 25 ms. The same section in Original averaged 59.91 and 59.92 FPS. Frame pacing varies with the course, display mode and PC.
