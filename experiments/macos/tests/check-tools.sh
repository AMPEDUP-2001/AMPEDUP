#!/bin/sh
# Run on a Mac with Xcode Command Line Tools. Requires no game data.
set -eu
here=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -f "$tmp/fixture" "$tmp/fixture.o" "$tmp/trace"; rmdir "$tmp"' EXIT

for mode in off on; do
    bit=0
    if [ "$mode" = on ]; then bit=1; fi
    RECOMP_DEBUG='fb_dump_at=61,121,cpuid_mmx=1' sh "$here/run-intro.sh" "$mode" sh -c '
        test "$RECOMP_DEBUG" = "fb_dump_at=61,121,cpuid_mmx=1,cpuid_mmx=$1" &&
        test "$RECOMP_PB_BACKEND/$RECOMP_PB_EXEC/$RECOMP_VBLANK" = metal/1/1 &&
        test "$AMPED_FENCE_EXPERIMENT/$AMPED_CONTIG_TOPDOWN" = 1/1 &&
        test "$2" = "path with spaces"
    ' sh "$bit" "path with spaces"
done
if sh "$here/run-intro.sh" other true 2>/dev/null; then exit 1; fi

xcrun clang -std=c11 -g -O0 -Wall -Wextra -Werror -c "$here/tests/watch_fixture.c" -o "$tmp/fixture.o"
xcrun clang "$tmp/fixture.o" -o "$tmp/fixture"
# Print one evidence row per stop, independent of LLDB's stack display format.
sample='script print("watch-sample", lldb.thread.GetStopReason() == lldb.eStopReasonWatchpoint, lldb.target.FindFirstGlobalVariable("callback").GetValueAsUnsigned())'
if ! xcrun lldb -b "$tmp/fixture" \
    -o "command script import \"$here/bink_watch.py\"" \
    -o 'breakpoint set --name xbox_kernel_init --one-shot true' -o run \
    -o amped-bink-watch -o continue -o "$sample" -o continue -o "$sample" -o continue \
    >"$tmp/trace" 2>&1; then
    cat "$tmp/trace"; exit 1
fi
cat "$tmp/trace"
grep -q 'initial=0x00000000, watchpoint' "$tmp/trace"
grep -qx 'watch-sample True 42' "$tmp/trace"
grep -qx 'watch-sample True 0' "$tmp/trace"
grep -q 'fixture finished' "$tmp/trace"
