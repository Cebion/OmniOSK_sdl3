#!/bin/sh
set -eu
library=$1
app=$2
trace=${TMPDIR:-/tmp}/omni-osk-renderer-$$.trace
trap 'rm -f "$trace"' EXIT
SDL_VIDEODRIVER=dummy LD_PRELOAD="$library" "$app" --trace "$trace" --events toggle,confirm,confirm,submit --duration-ms 500
test -f "$trace"
grep -q 'frame present=' "$trace"
grep -q 'sentinel first=' "$trace"
grep -q 'state backend=renderer restored=1' "$trace"
grep -q 'text=aa' "$trace"
if grep -q 'key=F12' "$trace" || test "$(grep -c 'key=Return' "$trace")" -ne 2; then
    echo "renderer preload leaked a consumed control"
    exit 1
fi
