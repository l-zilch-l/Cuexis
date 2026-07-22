option(CUEXIS_WARNINGS_AS_ERRORS "Treat Cuexis warnings as errors" OFF)

function(cuexis_enable_warnings target)
    get_target_property(target_type ${target} TYPE)
    if(target_type STREQUAL "INTERFACE_LIBRARY")
        set(scope INTERFACE)
    else()
        set(scope PRIVATE)
    endif()

    if(MSVC)
        target_compile_options(${target} ${scope}
            /W4
            /permissive-
            /Zc:__cplusplus
            /utf-8
        )
        target_compile_definitions(${target} ${scope} NOMINMAX WIN32_LEAN_AND_MEAN)
        if(CUEXIS_WARNINGS_AS_ERRORS)
            target_compile_options(${target} ${scope} /WX)
        endif()
    else()
        target_compile_options(${target} ${scope} -Wall -Wextra -Wpedantic)
        if(CUEXIS_WARNINGS_AS_ERRORS)
            target_compile_options(${target} ${scope} -Werror)
        endif()
    endif()
endfunction()
