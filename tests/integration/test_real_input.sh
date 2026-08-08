#!/bin/sh
set -eu
library=$1
gl_app=$2
renderer_app=$3
sender=$4
python=$5
checker=$6
if [ -z "${DISPLAY:-}" ]; then
    echo "SKIP: X11 display is unavailable"
    exit 77
fi

run_backend() {
    backend=$1
    title=$2
    app=$gl_app
    if [ "$backend" = renderer ]; then
        app=$renderer_app
    fi
    trace=${TMPDIR:-/tmp}/omni-osk-real-${backend}-$$.trace
    image=${TMPDIR:-/tmp}/omni-osk-real-${backend}-$$.ppm
    log=${TMPDIR:-/tmp}/omni-osk-real-${backend}-$$.log
    suffix="real-${backend}-$$"
    window_title="$title $suffix"
    trap 'kill "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true; rm -f "$trace" "$image" "$log"' EXIT HUP INT TERM
    OMNI_WINDOW_TITLE_SUFFIX="$suffix" OMNI_MODE=buffered OMNI_EVENT_MODE=text OMNI_CHARSETS=lower SDL_VIDEODRIVER=x11 LD_PRELOAD="$library" "$app" --trace "$trace" --duration-ms 2500 --screenshot "$image" >"$log" 2>&1 &
    pid=$!
    sleep 1.0
    "$sender" "$window_title" F12
    sleep 0.2
    "$sender" "$window_title" Return
    set +e
    wait "$pid"
    status=$?
    set -e
    if [ "$status" -eq 77 ]; then
        echo "SKIP: X11 $backend fixture is unavailable"
        return 77
    fi
    [ "$status" -eq 0 ]
    "$python" "$checker" "$backend" "$image" lower
    if grep -q 'key=F12' "$trace" || grep -q 'key=Return' "$trace"; then
        echo "$backend real input leaked a consumed key"
        return 1
    fi
    rm -f "$trace" "$image" "$log"
    trap - EXIT HUP INT TERM
}

run_submit() {
    backend=$1
    title=$2
    app=$gl_app
    if [ "$backend" = renderer ]; then
        app=$renderer_app
    fi
    trace=${TMPDIR:-/tmp}/omni-osk-real-submit-${backend}-$$.trace
    log=${TMPDIR:-/tmp}/omni-osk-real-submit-${backend}-$$.log
    suffix="real-submit-${backend}-$$"
    window_title="$title $suffix"
    trap 'kill "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true; rm -f "$trace" "$log"' EXIT HUP INT TERM
    OMNI_WINDOW_TITLE_SUFFIX="$suffix" OMNI_MODE=buffered OMNI_EVENT_MODE=text OMNI_CHARSETS=lower SDL_VIDEODRIVER=x11 LD_PRELOAD="$library" "$app" --trace "$trace" --duration-ms 3200 >"$log" 2>&1 &
    pid=$!
    sleep 1.0
    "$sender" "$window_title" F12
    sleep 0.2
    "$sender" "$window_title" Return
    sleep 0.2
    "$sender" "$window_title" Return
    sleep 0.2
    "$sender" "$window_title" Down
    sleep 0.2
    "$sender" "$window_title" Down
    sleep 0.2
    "$sender" "$window_title" Right
    sleep 0.2
    "$sender" "$window_title" Right
    sleep 0.2
    "$sender" "$window_title" Return
    set +e
    wait "$pid"
    status=$?
    set -e
    if [ "$status" -eq 77 ]; then
        echo "SKIP: X11 $backend submit fixture is unavailable"
        return 77
    fi
    [ "$status" -eq 0 ]
    if ! grep -q 'text=aa' "$trace"; then
        echo "$backend submit trace did not contain text=aa" >&2
        grep -E 'event type=|key=|text=' "$trace" >&2 || true
        return 1
    fi
    if grep -q 'key=F12' "$trace" || test "$(grep -c 'key=Return' "$trace")" -ne 2; then
        echo "$backend real submit input was not consumed correctly"
        return 1
    fi
    rm -f "$trace" "$log"
    trap - EXIT HUP INT TERM
}

run_backend gles3 "Omni OSK GLES sample"
run_backend renderer "Omni OSK SDL Renderer sample"
run_submit gles3 "Omni OSK GLES sample"
run_submit renderer "Omni OSK SDL Renderer sample"
