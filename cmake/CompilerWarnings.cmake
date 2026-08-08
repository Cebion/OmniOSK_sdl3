function(omni_enable_warnings target)
    if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion
        )
    elseif (MSVC)
        target_compile_options(${target} PRIVATE /W4)
    endif()
endfunction()
