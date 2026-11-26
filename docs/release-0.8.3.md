# AMPEDUP! 0.8.3

Potato Mode now spends less CPU time waiting for the game to draw each frame. If you already use Potato Mode, update and leave it on. To enable it for the first time, open **Options → Graphics → Potato Mode**, switch it **On** and confirm the restart.

- **Performance on slower PCs.** On Brighton, the previous Potato Mode averaged 9.43–9.62 displayed FPS on our Intel UHD 770 with the game limited to two CPU cores. The updated preset averaged 29.82 FPS on the same replay. A live Miracle Mile run on Snow Summit averaged 29.83 FPS. Some individual frames still took about 80 ms. The reporter’s Intel 620 laptop still needs a playtest.
- **Graphics and sound.** The rider shadow and snow spray stay on. A faster positional-audio filter produced identical output in a comparison of 262,144 samples. With Potato Mode off, our 4× ultrawide HD test on an RTX 3070 averaged 59.95 displayed FPS.
- **Fullscreen.** With Potato Mode and **Match fullscreen Hz** enabled, the 30 FPS cap prefers a compatible 60 Hz or higher refresh when the monitor offers one. Brighton averaged 29.15 displayed FPS in matched fullscreen on the two-core test.
- **Setup.** The installer and launcher now read the displayed version from the packaged executable. The setup window shows **0.8.3**. In RecompLauncher, let preparation finish so it can rebuild the updated game from your disc files.
