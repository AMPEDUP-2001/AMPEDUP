# Continue the Apple Silicon port

This branch contains the AMPEDUP! 0.7.3 source, its Metal renderer, and two diagnostic tools for an existing native Mac build. The public game entry point and installation guide still target Windows. The ARM64 executable and host changes described in issue #3 are on the contributor's Mac. A clean checkout of this branch alone does not build that executable. [Contributor's report](https://github.com/AMPEDUP-2001/AMPEDUP/issues/3#issuecomment-7131181262) · [Agent instructions](../../AGENTS.md)

## First test on the existing Mac build

The contributor reports that their M1 Max Mac Studio on macOS Tahoe reaches the opening Microsoft video. Metal presents a cleared frame. The log then reports a null call from guest address `0x00153E25`, returning to `0x00153E2B`. The Bink decoder reads its copy callback from `0x00242C4C`.

The Windows launcher explicitly enables `RECOMP_DEBUG=cpuid_mmx=1`. It is absent from the five settings listed in the Mac report. This switch changes the features the emulated Xbox CPU advertises to the decoder. The native build still runs on ARM64. [Windows launcher](../../Amped-Host/packaging/windows/launch.ps1) · [Guest CPUID implementation](../../Amped-Host/toolkit/src/kernel/kernel_hal.c)

Clone this branch beside the working Mac checkout. In Terminal, start in the directory where the native game already launches successfully. Substitute the two absolute paths below. Keep any usual game arguments after `"$game"`. Use the same test profile and do not skip the intro during the comparison.

```sh
git clone --single-branch --branch experimental/macos-issue-3 https://github.com/AMPEDUP-2001/AMPEDUP.git "$HOME/ampedup-mac-handoff"
tools="$HOME/ampedup-mac-handoff/experiments/macos"
game="/absolute/path/to/your/existing/amped_recomp"
cd "/your/usual/game/working/directory"
logs=$(mktemp -d)
echo "$logs"
sh "$tools/run-intro.sh" off "$game" >"$logs/mmx-off.log" 2>&1
sh "$tools/run-intro.sh" on  "$game" >"$logs/mmx-on.log" 2>&1
```

Close the first session after about 20 seconds or after the watchdog exits. Give the second at least the same time. If the movie starts advancing, let it reach the title. The launcher sets Metal and the four other environment settings from the reported run, preserves other debug options, and varies only the guest CPU feature switch between runs. It uses the binary and current directory you supply. For an existing launch script, verify it passes `RECOMP_DEBUG` through to the actual game.

On our Windows DEV build, we used the same executable, verified disc files, 4× resolution and muted audio for both runs, with a fresh profile each time. We sampled 18 presented frames per run over approximately the first 20 seconds. With MMX **off**, none matched either movie. With MMX **on**, eight matched the Microsoft logo and six matched advancing frames from the original snowboard movie. This gives the Mac experiment a Windows control. The Mac result is open.

## If the callback is still zero

1. Use the matching binary with debug symbols. Preserve the same startup environment. The wrapper can start LLDB with the same environment as the second run.

   ```sh
   sh "$tools/run-intro.sh" on xcrun lldb -- "$game"
   ```

2. In LLDB, use the actual absolute path to the Python module. The public startup maps Xbox memory before entering `xbox_kernel_init`. If the Mac host orders those steps differently, stop immediately after its mapping has completed and before guest code runs.

   ```text
   (lldb) command script import "/absolute/path/to/ampedup-mac-handoff/experiments/macos/bink_watch.py"
   (lldb) breakpoint set --name xbox_kernel_init --one-shot true
   (lldb) run
   (lldb) amped-bink-watch
   (lldb) continue
   (lldb) bt
   ```

   The command reads the mapping offset from this process and watches the four bytes of the callback for writes. Save the initial value, each old/new value and the short writer backtrace. Continue after each stop. If the slot stays zero, set a source breakpoint at the failing call too. Locate it with `grep -n '0x00153E25u' Analysis/seeded-v23/generated/recomp_0071.c` in the working checkout. LLDB's `breakpoint set --file recomp_0071.c --line N` takes the printed line number. If the watchdog fires during a debugger run, record that separately from the uninterrupted run.

3. A valid callback followed by a zero points to the later writer. If the callback is zero at first use and there were no observed writes, follow the decoder's initialization path and guest CPUID result. Hardcoding one candidate address hides the choice the game makes. If the callback is valid and execution passes the call, compare the first decoded frame, Metal draw and window presentation in order. There are separate drawing and window presentation paths. [Game callback diagnostic](../../Amped-Host/src/recomp_manual.c) · [Metal drawing](../../Amped-Host/toolkit/src/d3d/nv2a_pb_metal.m) · [Metal presentation](../../Amped-Host/toolkit/src/video/fb_present_metal.m)

## What to share on issue #3

Please share a fork or patch with your Mac host changes and build command, which test showed the first moving image, whether Start reaches the title, and the few log lines around the first failure. If you ran the watchpoint, include its first meaningful change and the writer's backtrace. Keep ISO/XBE files, generated guest code, movie files, saves and raw memory dumps on your machine. The public [disc regeneration script](../../Amped-Host/packaging/release/regenerate.py) and [Windows build guide](../../docs/building.md) document the source baseline.

To verify the tools independently, run `sh experiments/macos/tests/check-tools.sh` from the root of this checkout on a Mac with Xcode Command Line Tools. The test launches a synthetic process, catches a callback write and clear, and checks that the two launcher modes preserve arguments and debug settings. It needs no game files. We exercised the same watchpoint on Windows with LLDB and observed both writes (0 → 42 → 0). All 19 tests in the toolkit's `tools.recomp.test_lifter_eflags_cpuid` suite passed on Windows. The Mac tools check and the full game still need a run on macOS.
