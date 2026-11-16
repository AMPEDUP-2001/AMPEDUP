# AMPEDUP! 0.6.1

## What changed

AMPEDUP! batches eligible Xbox graphics commands before sending them through the existing runtime handlers. On Snow Summit 1 at 4×, the comparison recorded 89 long display intervals with batching and 118 on the previous path, over 92 measured seconds per mode. Both modes held 59.95 displayed FPS on the sampled section of Altibahn 1. A long interval is over 25 ms.

## 4× performance findings

The Windows development sampler now reads the rider's position about five times per second without changing the game executable. It tagged all 2,732 displayed frames in its two Snow Summit overhead runs, which averaged 59.43 and 59.34 FPS.

In two calibrated Snow Summit runs, 45 of 51 long intervals could be linked to both the rider's position and Vulkan work. The application's GPU queue finished ahead of the estimated refresh in all 45 cases, by a median of 16.88 ms. That puts the remaining delay after the game's GPU work in those samples. The exact presentation latch remains unmeasured. The Smooth FIFO trial produced 39 long intervals compared with 40 at the default over equal 46-second windows. Static geometry caching also showed no meaningful improvement. The release keeps the established presentation and geometry paths.

## AMPEDUP! so far

AMPEDUP! runs the original Xbox courses, physics, soundtrack, and game menus on Windows x64 using your own disc. Quick Start, Career saves, and replay playback have been tested with controller input. The Windows runtime offers Vulkan rendering, up to 4× internal resolution, anti-aliasing, CRT scanlines, and 4:3, 16:9, and optional 21:9 gameplay. The in-game settings include speaker layout, mix, and separate soundtrack and effects levels.

The Windows online and all-in-one installers and the RecompLauncher package compile the game from the supported European multilingual XISO. The online installer and RecompLauncher package download the pinned toolchain. The all-in-one installer supplies it locally.

## Next

Add custom soundtracks so players can use their own music in-game, as they could on the original Xbox.
