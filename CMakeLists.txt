cmake_minimum_required(VERSION 3.16)

project(omni_osk VERSION 0.1.0 LANGUAGES C CXX)

option(OMNI_BUILD_TESTS "Build unit and SDL fixture tests" ON)
option(OMNI_BUILD_EVDEV "Build the optional evdev/uinput backend" ON)
option(OMNI_BUILD_RENDERER_TEST "Build the SDL Renderer fixture" ${OMNI_BUILD_TESTS})

# This build targets the arcanumceosk port's SDL3-via-SDL2-backend shim
# (see /root/compile/sdl3-sdl2-backend), not real SDL2. The interception
# layer in src/preload.c speaks the SDL3 ABI and resolves the real
# implementation via that shim's exported SDL3SHIM_dlsym escape hatch
# (falls back to RTLD_NEXT for a plain SDL3 build without it) - see
# src/symbols.c. Everything else in this codebase (Dear ImGui's own SDL3
# backend, osk_view.cpp, etc.) links against SDL3 normally.
set(OMNI_SDL3_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../sdl3-sdl2-backend/include" CACHE PATH "Directory containing SDL3/SDL.h")
set(OMNI_SDL3_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/../sdl3-sdl2-backend/build/libSDL3.so" CACHE FILEPATH "Path to libSDL3.so to link against")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(GLESV2 REQUIRED IMPORTED_TARGET glesv2)
pkg_check_modules(LIBEVDEV QUIET IMPORTED_TARGET libevdev)

if (NOT EXISTS "${OMNI_SDL3_INCLUDE_DIR}/SDL3/SDL.h")
    message(FATAL_ERROR "SDL3 headers not found at ${OMNI_SDL3_INCLUDE_DIR}/SDL3/SDL.h - set -DOMNI_SDL3_INCLUDE_DIR")
endif()
if (NOT EXISTS "${OMNI_SDL3_LIBRARY}")
    message(FATAL_ERROR "libSDL3.so not found at ${OMNI_SDL3_LIBRARY} - build the sdl3-sdl2-backend shim first, or set -DOMNI_SDL3_LIBRARY")
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

set(OMNI_IMGUI_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
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
    "${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
    PROPERTIES COMPILE_OPTIONS "-w"
)

list(APPEND OMNI_IMGUI_SOURCES "${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp")
set_source_files_properties(
    "${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp"
    PROPERTIES COMPILE_OPTIONS "-w"
)

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
target_include_directories(omni_osk PRIVATE
    src
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
    "${OMNI_SDL3_INCLUDE_DIR}"
)
target_link_libraries(omni_osk PRIVATE
    "${OMNI_SDL3_LIBRARY}"
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
