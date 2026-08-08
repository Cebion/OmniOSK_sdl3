#!/bin/sh
set -eu
library=$1
app=$2
baseline=${TMPDIR:-/tmp}/omni-osk-baseline-$$.trace
trace=${TMPDIR:-/tmp}/omni-osk-preload-$$.trace
baseline_audit=${TMPDIR:-/tmp}/omni-osk-baseline-$$.audit
audit=${TMPDIR:-/tmp}/omni-osk-preload-$$.audit
trap 'rm -f "$baseline" "$trace" "$baseline_audit" "$audit"' EXIT

set +e
SDL_VIDEODRIVER=offscreen "$app" --trace "$baseline" --events game --duration-ms 500 >"$baseline_audit"
status=$?
set -e
if [ "$status" -eq 77 ]; then
    echo "SKIP: SDL OpenGL fixture is unavailable"
    exit 0
fi
[ "$status" -eq 0 ]
SDL_VIDEODRIVER=offscreen LD_PRELOAD="$library" "$app" --trace "$trace" --events game,toggle,game,confirm,confirm,submit --duration-ms 500 >"$audit"
test -f "$baseline"
test -f "$trace"
test "$(grep -c 'key=G' "$baseline")" -eq 2
grep -q 'APP_AUDIT_SUMMARY backend=gl keydown=1 keyup=1 text=0' "$baseline_audit"
grep -q 'text=aa' "$trace"
test "$(grep -c 'key=G' "$trace")" -eq 2
test "$(grep -c 'APP_RECEIVED backend=gl type=KEYDOWN key=G' "$audit")" -eq 1
test "$(grep -c 'APP_RECEIVED backend=gl type=KEYUP key=G' "$audit")" -eq 1
grep -q 'APP_RECEIVED backend=gl type=TEXTINPUT text=aa' "$audit"
grep -q 'APP_AUDIT_SUMMARY backend=gl keydown=2 keyup=2 text=1' "$audit"
if grep -q 'key=F12' "$trace" || test "$(grep -c 'key=Return' "$trace")" -ne 2; then
    echo "active preload leaked a keyboard event"
    exit 1
fi
test "$(grep -c 'frame present=' "$baseline")" -ge 2
test "$(grep -c 'frame present=' "$trace")" -ge 2
