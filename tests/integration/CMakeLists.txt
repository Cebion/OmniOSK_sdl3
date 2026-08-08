add_test(NAME integration_preload
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/test_preload.sh"
            "${CMAKE_BINARY_DIR}/lib/libomni_osk.so"
            "${CMAKE_BINARY_DIR}/tests/gles3/gles3")
add_test(NAME integration_fail_open
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/test_fail_open.sh"
            "${CMAKE_BINARY_DIR}/lib/libomni_osk.so"
            "${CMAKE_BINARY_DIR}/tests/gles3/gles3")
add_test(NAME integration_sdl_input
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/test_sdl_input.sh"
            "${CMAKE_BINARY_DIR}/lib/libomni_osk.so"
            "${CMAKE_BINARY_DIR}/tests/gles3/gles3")
add_test(NAME integration_evdev_backend
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/test_evdev_backend.sh")
if (OMNI_BUILD_RENDERER_TEST)
    add_test(NAME integration_renderer_overlay
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/test_renderer_overlay.sh"
                "${CMAKE_BINARY_DIR}/lib/libomni_osk.so"
                "${CMAKE_BINARY_DIR}/tests/renderer/renderer")
endif()
add_test(NAME integration_gl_overlay
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/test_gl_overlay.sh"
            "${CMAKE_BINARY_DIR}/lib/libomni_osk.so"
            "${CMAKE_BINARY_DIR}/tests/gles3/gles3")
find_package(Python3 QUIET COMPONENTS Interpreter)
if (Python3_Interpreter_FOUND AND OMNI_BUILD_RENDERER_TEST)
    add_test(NAME integration_visual_overlay
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/test_visual_overlay.sh"
                "${CMAKE_BINARY_DIR}/lib/libomni_osk.so"
                "${CMAKE_BINARY_DIR}/tests/gles3/gles3"
                "${CMAKE_BINARY_DIR}/tests/renderer/renderer"
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/check_keyboard_image.py")
    set_tests_properties(integration_visual_overlay PROPERTIES LABELS integration SKIP_RETURN_CODE 77)
endif()
if (TARGET send_x11_key AND Python3_Interpreter_FOUND AND OMNI_BUILD_RENDERER_TEST)
    add_test(NAME integration_real_input
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/test_real_input.sh"
                "${CMAKE_BINARY_DIR}/lib/libomni_osk.so"
                "${CMAKE_BINARY_DIR}/tests/gles3/gles3"
                "${CMAKE_BINARY_DIR}/tests/renderer/renderer"
                "${CMAKE_BINARY_DIR}/tests/tools/send_x11_key"
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/check_keyboard_image.py")
    set_tests_properties(integration_real_input PROPERTIES LABELS integration SKIP_RETURN_CODE 77)
endif()
set_tests_properties(integration_preload integration_fail_open integration_sdl_input integration_evdev_backend integration_gl_overlay PROPERTIES LABELS integration)
if (OMNI_BUILD_RENDERER_TEST)
    set_tests_properties(integration_renderer_overlay PROPERTIES LABELS integration)
endif()
