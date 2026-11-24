# AMPEDUP! experimental Mac contributor branch

Start with [the Mac handoff](experiments/macos/README.md). It records the contributor's M1 Max checkpoint, the first test to run, and the Windows comparison. The base is Windows release `v0.7.3` at `e6820c0abacff714efa3dc7c1f0fb27386cc26c5`.

- Use the contributor's existing native build, working directory, arguments and local disc files. Record their source revision, macOS version and exact build command first. Keep their edits intact. The public tree contains the release runtime, Metal backend and new diagnostic tools. The contributor's private ARM64 host changes have yet to be shared.
- First compare the two startup commands in the handoff. Change one setting per run. Record visible output and whether the specific Bink message recurs. A Metal clear or a successful build alone cannot verify movie playback.
- If both runs stall, trace the callback in the matching debug build. Establish the first differing write, call or frame before proposing a fix. Retest normal movie playback, Start-to-skip, the title menu and a short run with the diagnostic disabled.
- Work on a Mac feature branch and keep disc-derived files local. Share source patches, the build recipe and relevant log excerpts. Record separately what ran on the contributor's Mac and what ran in a hosted test.
