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
            /EHsc
        )
        target_compile_definitions(${target} ${scope} NOMINMAX WIN32_LEAN_AND_MEAN)
        if(CUEXIS_WARNINGS_AS_ERRORS)
            target_compile_options(${target} ${scope} /WX)
        endif()
    else()
        target_compile_options(${target} ${scope} -Wall -Wextra -Wpedantic)
        if(target MATCHES "_tests$" OR target MATCHES "_probe$")
            target_compile_options(${target} ${scope} -Wno-missing-field-initializers)
            if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
                # GCC 16 at -O3 reports a false -Wmaybe-uninitialized inside the libstdc++
                # std::string copy path when test fixtures copy a graph of CanonicalEntity values;
                # the diagnostic points at basic_string.h, not at project code. Downgrade only that
                # diagnostic so the -Werror release gate still fails on real findings. The option is
                # GCC-only: Clang has no -Wmaybe-uninitialized and rejects it as an unknown warning
                # option, which -Werror then turns into a build failure.
                target_compile_options(${target} ${scope} -Wno-error=maybe-uninitialized)
            endif()
        endif()
        if(CUEXIS_WARNINGS_AS_ERRORS)
            target_compile_options(${target} ${scope} -Werror)
        endif()
    endif()
endfunction()
