#!/bin/sh
set -eu
library=$1
app=$2
trace=${TMPDIR:-/tmp}/omni-osk-input-$$.trace
audit=${TMPDIR:-/tmp}/omni-osk-input-$$.audit
instant_trace=${TMPDIR:-/tmp}/omni-osk-instant-$$.trace
instant_audit=${TMPDIR:-/tmp}/omni-osk-instant-$$.audit
no_return_trace=${TMPDIR:-/tmp}/omni-osk-no-return-$$.trace
no_return_audit=${TMPDIR:-/tmp}/omni-osk-no-return-$$.audit
trap 'rm -f "$trace" "$audit" "$instant_trace" "$instant_audit" "$no_return_trace" "$no_return_audit"' EXIT

set +e
SDL_VIDEODRIVER=offscreen LD_PRELOAD="$library" "$app" --trace "$trace" --events toggle,confirm,confirm,submit --duration-ms 500 >"$audit"
status=$?
set -e
if [ "$status" -eq 77 ]; then
    echo "SKIP: SDL OpenGL fixture is unavailable"
    exit 0
fi
[ "$status" -eq 0 ]
test -f "$trace"
grep -q 'text=aa' "$trace"
grep -q 'APP_RECEIVED backend=gl type=TEXTINPUT text=aa' "$audit"
grep -q 'state backend=gl restored=1' "$trace"
if grep -q 'key=F12' "$trace" || test "$(grep -c 'key=Return' "$trace")" -ne 2; then
    echo "active SDL broker leaked a consumed control"
    exit 1
fi
test "$(grep -c 'frame present=' "$trace")" -ge 2

set +e
OMNI_MODE=instant SDL_VIDEODRIVER=offscreen LD_PRELOAD="$library" "$app" \
    --trace "$instant_trace" --events toggle,confirm,toggle --duration-ms 500 >"$instant_audit"
status=$?
set -e
[ "$status" -eq 0 ]
grep -q 'APP_RECEIVED backend=gl type=TEXTINPUT text=a' "$instant_audit"
grep -q 'APP_RECEIVED backend=gl type=KEYDOWN key=Return' "$instant_audit"
grep -q 'APP_RECEIVED backend=gl type=KEYUP key=Return' "$instant_audit"
if grep -q 'APP_RECEIVED backend=gl type=KEYDOWN key=F12' "$instant_audit" ||
   grep -q 'APP_RECEIVED backend=gl type=KEYUP key=F12' "$instant_audit"; then
    echo "instant SDL broker leaked the toggle control"
    exit 1
fi

set +e
OMNI_MODE=instant OMNI_EMIT_RETURN=0 SDL_VIDEODRIVER=offscreen LD_PRELOAD="$library" "$app" \
    --trace "$no_return_trace" --events toggle,confirm,toggle --duration-ms 500 >"$no_return_audit"
status=$?
set -e
[ "$status" -eq 0 ]
grep -q 'APP_RECEIVED backend=gl type=TEXTINPUT text=a' "$no_return_audit"
if grep -q 'APP_RECEIVED backend=gl type=KEYDOWN key=Return' "$no_return_audit" ||
   grep -q 'APP_RECEIVED backend=gl type=KEYUP key=Return' "$no_return_audit"; then
    echo "OMNI_EMIT_RETURN=0 leaked Return in instant mode"
    exit 1
fi
