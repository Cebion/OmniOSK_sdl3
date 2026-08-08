#!/bin/sh
set -eu
library=$1
app=$2
set +e
trace=${TMPDIR:-/tmp}/omni-osk-gl-$$.trace
trap 'rm -f "$trace"' EXIT
SDL_VIDEODRIVER=offscreen LD_PRELOAD="$library" "$app" --trace "$trace" --events toggle,confirm,confirm,submit --duration-ms 500
status=$?
set -e
[ "$status" -eq 0 ] || [ "$status" -eq 77 ]
if [ "$status" -eq 0 ]; then
    grep -q 'state backend=gl restored=1' "$trace"
    grep -q 'text=aa' "$trace"
fi
