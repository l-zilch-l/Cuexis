include_guard(GLOBAL)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR NOT CMAKE_GENERATOR MATCHES "^Ninja")
    return()
endif()

# Some MSVC installations emit UTF-8 even when CMake's compiler detection decodes
# /showIncludes using a different console code page. Only correct UTF-8 output;
# legacy non-UTF-8 output retains CMake's original encoding conversion.
set(cuexis_include_probe_dir "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/CuexisShowIncludes")
file(MAKE_DIRECTORY "${cuexis_include_probe_dir}")
file(WRITE "${cuexis_include_probe_dir}/cuexis_include_probe.hpp" "\n")
file(WRITE "${cuexis_include_probe_dir}/cuexis_include_probe.cpp"
    "#include \"cuexis_include_probe.hpp\"\n")
separate_arguments(cuexis_compiler_arg1 NATIVE_COMMAND "${CMAKE_CXX_COMPILER_ARG1}")
execute_process(
    COMMAND "${CMAKE_CXX_COMPILER}" ${cuexis_compiler_arg1}
        /nologo /showIncludes /c cuexis_include_probe.cpp
    WORKING_DIRECTORY "${cuexis_include_probe_dir}"
    OUTPUT_VARIABLE cuexis_include_probe_output
    ERROR_VARIABLE cuexis_include_probe_error
    RESULT_VARIABLE cuexis_include_probe_result
    ENCODING NONE)
if(NOT cuexis_include_probe_result EQUAL 0)
    message(FATAL_ERROR
        "MSVC include dependency probe failed (${cuexis_include_probe_result}): "
        "${cuexis_include_probe_error}")
endif()
if("${cuexis_include_probe_output}" MATCHES
   "(^|\n)([^:\n][^:\n]+:[^:\n]*[^: \n][^: \n]:?[ \t]+)([A-Za-z]:\\\\|\\./|\\.\\\\|/)")
    set(cuexis_include_raw_prefix "${CMAKE_MATCH_2}")
else()
    message(FATAL_ERROR "MSVC include dependency probe did not identify a /showIncludes prefix")
endif()
file(WRITE "${cuexis_include_probe_dir}/prefix.txt" "${cuexis_include_raw_prefix}")
file(STRINGS "${cuexis_include_probe_dir}/prefix.txt" cuexis_include_utf8_prefix ENCODING UTF-8)
if(cuexis_include_utf8_prefix STREQUAL cuexis_include_raw_prefix)
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "${cuexis_include_raw_prefix}")
    # Ninja emits its one global dependency prefix from the outermost directory.
    # Cuexis can also be embedded by add_subdirectory(), so apply it there after
    # all project() compiler configuration has finished.
    set_property(GLOBAL PROPERTY CUEXIS_MSVC_INCLUDE_PREFIX "${cuexis_include_raw_prefix}")
    function(cuexis_apply_msvc_include_prefix)
        get_property(prefix GLOBAL PROPERTY CUEXIS_MSVC_INCLUDE_PREFIX)
        set(CMAKE_CL_SHOWINCLUDES_PREFIX "${prefix}" PARENT_SCOPE)
    endfunction()
    cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL cuexis_apply_msvc_include_prefix)
endif()
unset(cuexis_include_probe_dir)
unset(cuexis_compiler_arg1)
unset(cuexis_include_probe_output)
unset(cuexis_include_probe_error)
unset(cuexis_include_probe_result)
unset(cuexis_include_raw_prefix)
unset(cuexis_include_utf8_prefix)
