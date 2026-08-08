set(OMNI_TEST_INCLUDE_DIRS
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/third_party/imgui"
    ${SDL2_INCLUDE_DIRS}
)

function(omni_add_unit name source)
    add_executable(${name} ${source} ${ARGN})
    target_include_directories(${name} PRIVATE ${OMNI_TEST_INCLUDE_DIRS})
    target_link_libraries(${name} PRIVATE PkgConfig::SDL2 Threads::Threads ${CMAKE_DL_LIBS})
    omni_enable_warnings(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

omni_add_unit(test_config unit/test_config.c ../src/config.c ../src/diagnostics.c)
set_tests_properties(test_config PROPERTIES LABELS config)
omni_add_unit(test_osk_model unit/test_osk_model.c ../src/config.c ../src/charset.c ../src/osk_model.c ../src/diagnostics.c)
set_tests_properties(test_osk_model PROPERTIES LABELS osk_model)
omni_add_unit(test_sdl_events unit/test_sdl_events.c ../src/config.c ../src/charset.c ../src/osk_model.c ../src/generated_events.c ../src/input_sdl.c ../src/diagnostics.c)
set_tests_properties(test_sdl_events PROPERTIES LABELS sdl_events)
omni_add_unit(test_symbols unit/test_symbols.c ../src/diagnostics.c ../src/symbols.c)
set_tests_properties(test_symbols PROPERTIES LABELS symbols)
omni_add_unit(test_evdev_protocol unit/test_evdev_protocol.c ../src/config.c ../src/uinput_keyboard.c ../src/input_evdev.c ../src/diagnostics.c)
set_tests_properties(test_evdev_protocol PROPERTIES LABELS evdev_protocol)

add_executable(gles3 apps/gles3/main.c apps/fixture.c)
target_include_directories(gles3 PRIVATE ${SDL2_INCLUDE_DIRS} ${GLESV2_INCLUDE_DIRS} "${CMAKE_SOURCE_DIR}/tests/apps")
target_link_libraries(gles3 PRIVATE PkgConfig::SDL2 ${GLESV2_LIBRARIES} m)
omni_enable_warnings(gles3)
set_target_properties(gles3 PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/gles3")

if (OMNI_BUILD_RENDERER_TEST)
    add_executable(renderer apps/renderer/main.c apps/fixture.c)
    target_include_directories(renderer PRIVATE ${SDL2_INCLUDE_DIRS} ${GLESV2_INCLUDE_DIRS} "${CMAKE_SOURCE_DIR}/tests/apps")
    target_link_libraries(renderer PRIVATE PkgConfig::SDL2 ${GLESV2_LIBRARIES} m)
    omni_enable_warnings(renderer)
    set_target_properties(renderer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/renderer")
endif()

pkg_check_modules(X11_XTEST QUIET x11 xtst)
if (X11_XTEST_FOUND)
    add_executable(send_x11_key tools/send_x11_key.c)
    target_include_directories(send_x11_key PRIVATE ${X11_XTEST_INCLUDE_DIRS})
    target_link_directories(send_x11_key PRIVATE ${X11_XTEST_LIBRARY_DIRS})
    target_link_libraries(send_x11_key PRIVATE ${X11_XTEST_LIBRARIES})
    omni_enable_warnings(send_x11_key)
    set_target_properties(send_x11_key PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/tools")
endif()

add_subdirectory(integration)
