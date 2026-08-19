cmake_minimum_required(VERSION 3.16)

project(omni_osk VERSION 0.1.0 LANGUAGES C CXX)

option(OMNI_BUILD_TESTS "Build unit and SDL fixture tests" ON)
option(OMNI_BUILD_EVDEV "Build the optional evdev/uinput backend" ON)
option(OMNI_BUILD_RENDERER_TEST "Build the SDL Renderer fixture" ${OMNI_BUILD_TESTS})
set(OMNI_SDL2_MIN_VERSION "2.0.18" CACHE STRING "Minimum SDL2 version for the Dear ImGui SDL Renderer backend")

# For a target reached only through sdl12-compat: the game (and sdl12-compat
# itself) speak real SDL 1.2's ABI, which has a completely different
# SDL_Event memory layout than SDL2 (Uint8 type vs Uint32 type, among other
# differences) - reading events with this build's normal SDL2-typed code
# silently misreads garbage. When ON, src/sdl12_bridge.c is compiled
# separately against real SDL 1.2 headers to decode the raw event buffer
# correctly before the rest of this SDL2-typed codebase ever touches it.
# Leave OFF for a genuinely SDL2-native target game.
option(OMNI_SDL12_BRIDGE "Decode events using real SDL 1.2 struct layout (for games reached via sdl12-compat)" OFF)
set(OMNI_SDL12_INCLUDE_DIR "/usr/include" CACHE PATH "Directory containing SDL/SDL.h (real SDL 1.2 headers)")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
find_package(Threads REQUIRED)
pkg_check_modules(GLESV2 REQUIRED IMPORTED_TARGET glesv2)
pkg_check_modules(LIBEVDEV QUIET IMPORTED_TARGET libevdev)

if (SDL2_VERSION VERSION_LESS OMNI_SDL2_MIN_VERSION)
    message(FATAL_ERROR "SDL2 ${SDL2_VERSION} is too old; this project requires SDL2 >= ${OMNI_SDL2_MIN_VERSION} for Dear ImGui SDL Renderer2")
endif()

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")
if (NOT EXISTS "${IMGUI_DIR}/imgui.cpp" OR NOT EXISTS "${IMGUI_DIR}/VERSION")
    message(FATAL_ERROR "Dear ImGui is not initialized. Run: git submodule update --init --recursive")
endif()

set(OMNI_CORE_SOURCES
    src/config.c
    src/charset.c
    src/osk_model.c
    src/diagnostics.c
    src/symbols.c
    src/runtime.c
    src/preload.c
    src/input_sdl.c
    src/generated_events.c
    src/gl_loader.c
)
if (OMNI_BUILD_EVDEV)
    list(APPEND OMNI_CORE_SOURCES src/input_evdev.c src/uinput_keyboard.c)
endif()
if (OMNI_SDL12_BRIDGE)
    if (NOT EXISTS "${OMNI_SDL12_INCLUDE_DIR}/SDL/SDL.h")
        message(FATAL_ERROR "SDL 1.2 headers not found at ${OMNI_SDL12_INCLUDE_DIR}/SDL/SDL.h - set -DOMNI_SDL12_INCLUDE_DIR")
    endif()
    list(APPEND OMNI_CORE_SOURCES src/sdl12_bridge.c)
    set_source_files_properties(src/sdl12_bridge.c PROPERTIES
        INCLUDE_DIRECTORIES "${OMNI_SDL12_INCLUDE_DIR}"
    )
endif()

set(OMNI_IMGUI_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
    src/imgui_bridge.cpp
    src/osk_view.cpp
    src/render_gl.cpp
    src/render_sdlrenderer.cpp
)
set_source_files_properties(
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
    PROPERTIES COMPILE_OPTIONS "-w"
)

list(APPEND OMNI_IMGUI_SOURCES "${IMGUI_DIR}/backends/imgui_impl_sdlrenderer2.cpp")

add_library(omni_osk SHARED ${OMNI_CORE_SOURCES} ${OMNI_IMGUI_SOURCES})
set_target_properties(omni_osk PROPERTIES
    OUTPUT_NAME omni_osk
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
target_compile_definitions(omni_osk PRIVATE
    IMGUI_IMPL_OPENGL_ES2
    IMGUI_IMPL_OPENGL_LOADER_CUSTOM
    _GNU_SOURCE
)
if (OMNI_BUILD_EVDEV)
    target_compile_definitions(omni_osk PRIVATE OMNI_HAVE_EVDEV=1)
endif()
if (OMNI_SDL12_BRIDGE)
    target_compile_definitions(omni_osk PRIVATE OMNI_HAVE_SDL12_BRIDGE=1)
endif()
target_include_directories(omni_osk PRIVATE
    src
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
    ${SDL2_INCLUDE_DIRS}
)
target_link_libraries(omni_osk PRIVATE
    PkgConfig::SDL2
    Threads::Threads
    ${CMAKE_DL_LIBS}
)
target_include_directories(omni_osk PRIVATE ${GLESV2_INCLUDE_DIRS})
if (OMNI_BUILD_EVDEV AND LIBEVDEV_FOUND)
    target_compile_definitions(omni_osk PRIVATE OMNI_HAVE_LIBEVDEV=1)
    target_include_directories(omni_osk PRIVATE ${LIBEVDEV_INCLUDE_DIRS})
    target_link_libraries(omni_osk PRIVATE PkgConfig::LIBEVDEV)
endif()
omni_enable_warnings(omni_osk)

if (OMNI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)
install(TARGETS omni_osk LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DOCDIR})
