#!/bin/sh
set -eu
library=$1
gl_app=$2
renderer_app=$3
python=$4
checker=$5

run_case() {
    backend=$1
    driver=$2
    page=$3
    minimum_keys=$4
    mode=${5:-buffered}
    app=$gl_app
    if [ "$backend" = renderer ]; then
        app=$renderer_app
    fi
    trace=${TMPDIR:-/tmp}/omni-osk-visual-${backend}-$$.trace
    image=${TMPDIR:-/tmp}/omni-osk-visual-${backend}-$$.ppm
    set +e
    OMNI_MODE="$mode" OMNI_CHARSETS="$page" SDL_VIDEODRIVER="$driver" LD_PRELOAD="$library" "$app" \
        --trace "$trace" --events toggle,confirm --duration-ms 300 --screenshot "$image"
    status=$?
    set -e
    if [ "$status" -eq 77 ]; then
        echo "SKIP: $backend fixture is unavailable"
        return 77
    fi
    [ "$status" -eq 0 ]
    "$python" "$checker" "$backend" "$image" "$page" "$minimum_keys"
    state_backend=renderer
    if [ "$backend" = gles3 ]; then
        state_backend=gl
    fi
    grep -q "state backend=$state_backend restored=1" "$trace"
    rm -f "$trace" "$image"
}

run_case gles3 offscreen lower 8
run_case renderer dummy lower 8
run_case gles3 offscreen number 3
run_case renderer dummy special 6
run_case renderer dummy lower 8 instant
