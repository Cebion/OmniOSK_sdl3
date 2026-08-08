# OmniOSK

OmniOSK is a standalone on-screen keyboard for controller-first SDL2
applications. It is loaded with `LD_PRELOAD`, so a PortMaster launch script can
add text entry without changing the application or linking against an SDK.

The overlay presents a configurable QWERTY keyboard at presentation time and
passes ordinary application input through unchanged while it is closed. It
supports SDL input by default, an optional evdev/uinput input path, OpenGL ES
and SDL Renderer presentation, buffered or instant text entry, and configurable
appearance and controls.

## Features

- Standalone `libomni_osk.so` preload for SDL2 applications
- Controller-friendly QWERTY layout with configurable character pages
- Buffered submission or immediate character output
- SDL text-input or keyboard-event output
- OpenGL ES and SDL Renderer presentation paths
- Configurable placement, sizing, colors, font, and navigation keys
- Fail-open behavior when the overlay cannot initialize safely

## Requirements

- Linux
- CMake 3.16 or newer
- C11 and C++11 compilers
- SDL2 development files, version 2.0.18 or newer
- GLES2 headers, pthreads, and `libdl`
- Optional evdev/uinput support can be disabled with `OMNI_BUILD_EVDEV=OFF`

Dear ImGui is included as a git submodule. Initialize it before configuring the
project:

```sh
git submodule update --init --recursive
```

## Build

Configure, build, and run the test suite with:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DOMNI_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The shared library is written to `build/lib/libomni_osk.so`. The SDL fixture
applications are written to `build/tests/gles3/gles3` and
`build/tests/renderer/renderer`.

To install the library and README into a prefix:

```sh
cmake --install build --prefix /path/to/prefix
```

The evdev backend can be disabled for a build that only needs SDL input:

```sh
cmake -S . -B build -DOMNI_BUILD_EVDEV=OFF
```

## PortMaster Usage

Add the preload to the port's existing launch environment. The target must be
an SDL2 application.

```sh
export LD_PRELOAD="${PORT_DIR}/lib/libomni_osk.so${LD_PRELOAD:+:$LD_PRELOAD}"
export OMNI_MODE=buffered
export OMNI_EVENT_MODE=auto
exec "${PORT_DIR}/game"
```

The default toggle key is `F12`. While the keyboard is open, arrow keys move
focus, `Enter` confirms, `Tab` changes character pages, `Backspace` deletes,
and `F12` dismisses. The toggle key is reserved; ordinary input passes through
when the overlay is closed and is consumed by the overlay when it is open.

## Configuration

Configuration is read once at launch from `OMNI_*` environment variables.
Unknown or invalid values produce a warning and use their defaults.

### Input And Controls

| Variable | Values and default |
| --- | --- |
| `OMNI_INPUT_BACKEND` | `sdl`, `evdev`, or `auto`; default `sdl` |
| `OMNI_INPUT_FALLBACK` | `0` or `1`; default `1` |
| `OMNI_EVDEV_DEVICE` | `auto` or an evdev path; default `auto` |
| `OMNI_TOGGLE_KEY` | SDL key name; default `F12` |
| `OMNI_UP_KEY` / `OMNI_DOWN_KEY` | key names; defaults `UP` / `DOWN` |
| `OMNI_LEFT_KEY` / `OMNI_RIGHT_KEY` | key names; defaults `LEFT` / `RIGHT` |
| `OMNI_CONFIRM_KEY` | key name; default `RETURN` |
| `OMNI_BACKSPACE_KEY` | key name; default `BACKSPACE` |
| `OMNI_CHARSET_KEY` | key name; default `TAB` |

### Text Entry

| Variable | Values and default |
| --- | --- |
| `OMNI_MODE` | `buffered` or `instant`; default `buffered` |
| `OMNI_EVENT_MODE` | `auto`, `text`, or `key`; default `auto` |
| `OMNI_BUFFER_DISMISS` | `stay`, `submit`, or `cancel`; default `cancel` |
| `OMNI_BUFFER_LIMIT` | `1` to `4096`; default `256` |
| `OMNI_OPEN_BACKSPACE_COUNT` | `0` to `256` Backspace pairs on open; default `0` |
| `OMNI_EMIT_RETURN` | `0` or `1`; default `1` |
| `OMNI_CHARSETS` | Semicolon-separated `lower`, `upper`, `number`, and `special` pages; default `lower;upper;number;special` |

Buffered mode emits the completed value when Submit is selected. Instant mode
emits each selected character and uses Close instead of Submit and Cancel. The
special page includes Space, and Backspace is available on every page.

### Layout And Appearance

| Variable | Values and default |
| --- | --- |
| `OMNI_ANCHOR` | One of nine standard anchors; default `bottom-center` |
| `OMNI_WIDTH` / `OMNI_HEIGHT` | Normalized `0.05` to `1.0`; defaults `0.90` / `0.55` |
| `OMNI_X` / `OMNI_Y` | Normalized offsets `-1.0` to `1.0`; default `0` |
| `OMNI_OPACITY` | `0.0` to `1.0`; default `0.92` |
| `OMNI_STYLE` | `default`, `compact`, or `high-contrast`; default `default` |
| `OMNI_PANEL_COLOR` / `OMNI_KEY_COLOR` | `RRGGBB` or `RRGGBBAA`; defaults follow `OMNI_STYLE` |
| `OMNI_FOCUS_COLOR` / `OMNI_BORDER_COLOR` | `RRGGBB` or `RRGGBBAA`; defaults follow `OMNI_STYLE` |
| `OMNI_TEXT_COLOR` | `RRGGBB` or `RRGGBBAA`; default follows `OMNI_STYLE` |
| `OMNI_CANCEL_COLOR` / `OMNI_SUBMIT_COLOR` / `OMNI_CLOSE_COLOR` | Action colors as `RRGGBB` or `RRGGBBAA` |
| `OMNI_WINDOW_ROUNDING` / `OMNI_KEY_ROUNDING` | Pixel rounding values; defaults follow `OMNI_STYLE` |
| `OMNI_KEY_GAP` / `OMNI_ROW_GAP` | Pixel spacing values; defaults follow `OMNI_STYLE` |
| `OMNI_WINDOW_PADDING` | Pixel panel padding; default follows `OMNI_STYLE` |
| `OMNI_FONT` | Path to a TrueType font; default uses Dear ImGui's built-in font |
| `OMNI_FONT_SIZE` | `8.0` to `64.0` pixels when `OMNI_FONT` is set |
| `OMNI_LOG` | `error`, `warn`, `info`, or `debug`; default `warn` |

## Cross Compilation

For an aarch64 target, provide the cross compiler and target-rooted SDL2,
GLES2, and pkg-config files:

```sh
export OMNI_SYSROOT=/path/to/aarch64/sysroot
export PKG_CONFIG_SYSROOT_DIR=/path/to/aarch64/sysroot
export PKG_CONFIG_LIBDIR=/path/to/aarch64/sysroot/usr/lib/aarch64-linux-gnu/pkgconfig:/path/to/aarch64/sysroot/usr/share/pkgconfig

cmake -S . -B build-aarch64 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake \
  -DOMNI_BUILD_RENDERER_TEST=OFF \
  -DOMNI_BUILD_TESTS=OFF
cmake --build build-aarch64
```

## Development

Unit and integration tests are enabled by default. The test suite includes
configuration, keyboard-model, SDL event, rendering, and input-backend
coverage. Hardware-dependent checks skip when their required display or input
device capabilities are unavailable.

Run a focused set of unit tests with:

```sh
ctest --test-dir build --output-on-failure \
  -R 'config|osk_model|sdl_events|evdev_protocol'
```
