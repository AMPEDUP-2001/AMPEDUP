#!/bin/sh
# Launch the contributor's native build from its normal working directory.
set -eu
if [ "$#" -lt 2 ]; then
    echo 'Usage: run-intro.sh off|on GAME_EXECUTABLE [GAME_ARGUMENT ...]' >&2
    exit 2
fi
case "$1" in
    off) mmx=0 ;;
    on)  mmx=1 ;;
    *)   echo 'Select off or on for the guest MMX test.' >&2; exit 2 ;;
esac
shift
export RECOMP_PB_BACKEND=metal RECOMP_PB_EXEC=1 RECOMP_VBLANK=1
export AMPED_FENCE_EXPERIMENT=1 AMPED_CONTIG_TOPDOWN=1
# The runtime processes this list left to right. Keep other options, including
# values containing commas, then give this run the final say on cpuid_mmx.
export RECOMP_DEBUG="${RECOMP_DEBUG:+$RECOMP_DEBUG,}cpuid_mmx=$mmx"
printf '[macOS intro test] Metal, guest cpuid_mmx=%s\n' "$mmx" >&2
exec "$@"
