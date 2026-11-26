# AMPEDUP! 0.8.2

If the menus feel smooth but riding looks choppy, you can now try matching the monitor to the game. Open **Options → Graphics**, turn on **Fullscreen** and **Match fullscreen Hz**, leave **VSync** on, and set **FPS cap** to **60**. You can also open Graphics with **F1** or **View + Menu** on an Xbox controller.

- **Match fullscreen Hz** is optional and takes effect immediately. It keeps the desktop resolution and asks the monitor for the selected cap or its lowest supported whole multiple. For example, a 60 FPS cap can use 120 Hz if 60 Hz is unavailable. With the cap off or no compatible rate, the game uses the desktop refresh rate. Leaving fullscreen or quitting restores the desktop mode.
- **VSync** now has its own switch in Graphics and in RecompLauncher. It starts on. Changing it in-game asks you to restart. Turning it off permits tearing on supported systems.
- **Updating through RecompLauncher** rebuilds the Windows runtime from your game files. Let preparation finish before pressing Play. Standalone installers are available in online and all-in-one versions.

On our RTX 3070, the same 36 seconds of a Brighton replay at 4× with the HD pack averaged about 60 displayed FPS in both modes. The 99th-percentile display interval improved from **20.07 ms** at the desktop’s 100 Hz to **16.72 ms** with refresh matching. This was a desktop test. Feedback from laptop displays is welcome.
