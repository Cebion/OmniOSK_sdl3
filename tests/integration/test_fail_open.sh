#!/bin/sh
set -eu
library=$1
app=$2
trace=${TMPDIR:-/tmp}/omni-osk-fail-open-$$.trace
trap 'rm -f "$trace"' EXIT

set +e
SDL_VIDEODRIVER=offscreen LD_PRELOAD="$library" OMNI_INPUT_BACKEND=evdev OMNI_INPUT_FALLBACK=0 OMNI_EVDEV_DEVICE=/does/not/exist "$app" --trace "$trace" --events toggle,confirm,confirm,submit --duration-ms 300
status=$?
set -e
if [ "$status" -eq 77 ]; then
    echo "SKIP: SDL OpenGL fixture is unavailable"
    exit 0
fi
[ "$status" -eq 0 ]
grep -q 'state backend=gl restored=1' "$trace"
grep -q 'key=F12' "$trace"
test "$(grep -c 'key=Return' "$trace")" -eq 6
