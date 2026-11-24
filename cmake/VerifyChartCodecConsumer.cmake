# Verifies the RR-F302 pure codec consumer as its own CMake project.
#
# The consumer links `Cuexis::Core` and the Chart codec only. The positive translation unit holds
# `chart::CompiledChartTiming` and compiles a real timing document, so it proves the build-only
# internal Chart timing root reaches the codec consumer through the effect of the actual link
# closure. The two negative translation units include headers that belong to `cuexis_chart_tools`;
# each is built in its own configuration and must fail, which proves the tools include root is not
# part of this consumer.
#
# Required:
#   CUEXIS_SOURCE_DIR          repository root
#   CUEXIS_BINARY_DIR          build directory of the inspected configuration (default work root)
#   CUEXIS_BUILD_TYPE          configuration to build, e.g. Debug
#   CUEXIS_GENERATOR           CMake generator name
# Optional:
#   CUEXIS_CONSUMER_WORK_ROOT  where the consumer build directories are created
#   CUEXIS_CXX_COMPILER, CUEXIS_MAKE_PROGRAM, CUEXIS_RC_COMPILER, CUEXIS_MT
#   CUEXIS_TOOLCHAIN_FILE      CMake toolchain file used by the producer build
#   CUEXIS_VCPKG_TARGET_TRIPLET, CUEXIS_VCPKG_INSTALLED_DIR
#   CUEXIS_REPORT_FILE         append the check log to this file
#
# Usage:
#   cmake -DCUEXIS_SOURCE_DIR=<root> -DCUEXIS_BINARY_DIR=<build> -DCUEXIS_BUILD_TYPE=Debug \
#         -DCUEXIS_GENERATOR=Ninja -DCUEXIS_TOOLCHAIN_FILE=$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
#         -DCUEXIS_VCPKG_TARGET_TRIPLET=x64-windows \
#         -DCUEXIS_VCPKG_INSTALLED_DIR=<build>/vcpkg_installed \
#         -P cmake/VerifyChartCodecConsumer.cmake
#
# A failing positive configure/build/run, or a negative translation unit that compiles, is itself a
# failure: the consumer must prove the split, not merely avoid including the tools headers.

foreach(required_variable IN ITEMS CUEXIS_SOURCE_DIR CUEXIS_BINARY_DIR CUEXIS_BUILD_TYPE
        CUEXIS_GENERATOR)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "VerifyChartCodecConsumer: ${required_variable} is required")
    endif()
endforeach()

if(NOT DEFINED CUEXIS_CONSUMER_WORK_ROOT OR CUEXIS_CONSUMER_WORK_ROOT STREQUAL "")
    set(CUEXIS_CONSUMER_WORK_ROOT "${CUEXIS_BINARY_DIR}/rr-chart-codec-consumer")
endif()
if(NOT DEFINED CUEXIS_REPORT_FILE)
    set(CUEXIS_REPORT_FILE "")
endif()

function(cuexis_report line)
    if(NOT CUEXIS_REPORT_FILE STREQUAL "")
        file(APPEND "${CUEXIS_REPORT_FILE}" "${line}\n")
    endif()
endfunction()

if(NOT CUEXIS_REPORT_FILE STREQUAL "")
    file(WRITE "${CUEXIS_REPORT_FILE}" "")
endif()

function(cuexis_run_checked description)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE run_result OUTPUT_VARIABLE run_output
        ERROR_VARIABLE run_error)
    if(NOT run_result EQUAL 0)
        message(FATAL_ERROR
            "VerifyChartCodecConsumer: ${description} failed (${run_result})\n"
            "stdout:\n${run_output}\nstderr:\n${run_error}")
    endif()
    set(cuexis_last_output "${run_output}" PARENT_SCOPE)
    set(cuexis_last_error "${run_error}" PARENT_SCOPE)
endfunction()

set(configure_arguments
    -G "${CUEXIS_GENERATOR}"
    "-DCMAKE_BUILD_TYPE=${CUEXIS_BUILD_TYPE}"
    "-DCUEXIS_SOURCE_DIR=${CUEXIS_SOURCE_DIR}"
)

set(producer_variable_mappings
    CUEXIS_CXX_COMPILER|CMAKE_CXX_COMPILER
    CUEXIS_MAKE_PROGRAM|CMAKE_MAKE_PROGRAM
    CUEXIS_RC_COMPILER|CMAKE_RC_COMPILER
    CUEXIS_MT|CMAKE_MT
    CUEXIS_TOOLCHAIN_FILE|CMAKE_TOOLCHAIN_FILE
)
foreach(mapping IN LISTS producer_variable_mappings)
    string(REPLACE "|" ";" mapping_parts "${mapping}")
    list(GET mapping_parts 0 source_variable)
    list(GET mapping_parts 1 target_variable)
    if(DEFINED ${source_variable} AND NOT "${${source_variable}}" STREQUAL "")
        list(APPEND configure_arguments "-D${target_variable}=${${source_variable}}")
    endif()
endforeach()
if(DEFINED CUEXIS_TOOLCHAIN_FILE AND NOT CUEXIS_TOOLCHAIN_FILE STREQUAL "")
    # The consumer is its own project and must not reinstall the repository manifest; it borrows
    # the producer's already-installed dependency set.
    list(APPEND configure_arguments -DVCPKG_MANIFEST_MODE=OFF)
    if(DEFINED CUEXIS_VCPKG_TARGET_TRIPLET AND NOT CUEXIS_VCPKG_TARGET_TRIPLET STREQUAL "")
        list(APPEND configure_arguments "-DVCPKG_TARGET_TRIPLET=${CUEXIS_VCPKG_TARGET_TRIPLET}")
    endif()
    if(DEFINED CUEXIS_VCPKG_INSTALLED_DIR AND NOT CUEXIS_VCPKG_INSTALLED_DIR STREQUAL "")
        list(APPEND configure_arguments "-DVCPKG_INSTALLED_DIR=${CUEXIS_VCPKG_INSTALLED_DIR}")
    endif()
elseif(DEFINED CUEXIS_VCPKG_INSTALLED_DIR AND NOT CUEXIS_VCPKG_INSTALLED_DIR STREQUAL ""
       AND DEFINED CUEXIS_VCPKG_TARGET_TRIPLET AND NOT CUEXIS_VCPKG_TARGET_TRIPLET STREQUAL "")
    list(APPEND configure_arguments
        "-DCMAKE_PREFIX_PATH=${CUEXIS_VCPKG_INSTALLED_DIR}/${CUEXIS_VCPKG_TARGET_TRIPLET}")
endif()

if(WIN32)
    set(executable_suffix ".exe")
else()
    set(executable_suffix "")
endif()

set(configure_arguments_text "${configure_arguments}")
string(REPLACE ";" " " configure_arguments_text "${configure_arguments_text}")

file(REMOVE_RECURSE "${CUEXIS_CONSUMER_WORK_ROOT}")
file(MAKE_DIRECTORY "${CUEXIS_CONSUMER_WORK_ROOT}")

set(consumer_source "${CUEXIS_SOURCE_DIR}/tests/consumers/chart-codec")
if(NOT EXISTS "${consumer_source}/CMakeLists.txt")
    message(FATAL_ERROR "VerifyChartCodecConsumer: missing consumer project at '${consumer_source}'")
endif()

cuexis_report("# VerifyChartCodecConsumer")
cuexis_report("source_dir=${CUEXIS_SOURCE_DIR}")
cuexis_report("configure: -S ${consumer_source} ${configure_arguments_text}")
cuexis_report("")

# Positive: the codec consumer compiles, links and runs, and its link closure names the codec
# archive while staying free of the tools, Playback and compile-side archives.
set(positive_build_dir "${CUEXIS_CONSUMER_WORK_ROOT}/positive")
cuexis_run_checked("positive configure"
    "${CMAKE_COMMAND}" -S "${consumer_source}" -B "${positive_build_dir}" ${configure_arguments})
cuexis_report("positive configure: exit=0")
set(positive_build_command
    "${CMAKE_COMMAND}" --build "${positive_build_dir}" --target rr_chart_codec_consumer --verbose)
execute_process(COMMAND ${positive_build_command} RESULT_VARIABLE positive_build_result
    OUTPUT_VARIABLE positive_build_output ERROR_VARIABLE positive_build_error)
set(positive_build_log "${positive_build_output}\n${positive_build_error}")
cuexis_report("positive build: exit=${positive_build_result}")
if(NOT positive_build_result EQUAL 0)
    message(FATAL_ERROR
        "VerifyChartCodecConsumer: positive build failed (${positive_build_result})\n"
        "${positive_build_log}")
endif()
if(NOT positive_build_log MATCHES "cuexis_chart_codec")
    message(FATAL_ERROR
        "VerifyChartCodecConsumer: the positive build does not link the Chart codec archive\n"
        "${positive_build_log}")
endif()
cuexis_report("positive build links cuexis_chart_codec: yes")
foreach(forbidden_archive IN ITEMS cuexis_chart_tools cuexis_judgement_compile cuexis_playback
        cuexis_gameplay_packed)
    if(positive_build_log MATCHES "${forbidden_archive}")
        message(FATAL_ERROR
            "VerifyChartCodecConsumer: the positive consumer build reaches the unrelated archive "
            "'${forbidden_archive}'")
    endif()
    cuexis_report("positive build reaches ${forbidden_archive}: no")
endforeach()

set(positive_program "${positive_build_dir}/rr_chart_codec_consumer${executable_suffix}")
if(NOT EXISTS "${positive_program}")
    message(FATAL_ERROR
        "VerifyChartCodecConsumer: positive consumer executable is missing at '${positive_program}'")
endif()
if(WIN32 AND DEFINED CUEXIS_VCPKG_INSTALLED_DIR AND NOT CUEXIS_VCPKG_INSTALLED_DIR STREQUAL ""
   AND DEFINED CUEXIS_VCPKG_TARGET_TRIPLET AND NOT CUEXIS_VCPKG_TARGET_TRIPLET STREQUAL "")
    set(ENV{PATH} "${CUEXIS_VCPKG_INSTALLED_DIR}/${CUEXIS_VCPKG_TARGET_TRIPLET}/debug/bin;\
${CUEXIS_VCPKG_INSTALLED_DIR}/${CUEXIS_VCPKG_TARGET_TRIPLET}/bin;$ENV{PATH}")
endif()
execute_process(COMMAND "${positive_program}" RESULT_VARIABLE positive_run_result
    OUTPUT_VARIABLE positive_run_output ERROR_VARIABLE positive_run_error)
cuexis_report("positive run: exit=${positive_run_result} output=${positive_run_output}")
if(NOT positive_run_result EQUAL 0)
    message(FATAL_ERROR
        "VerifyChartCodecConsumer: positive consumer failed at runtime (${positive_run_result})\n"
        "${positive_run_output}\n${positive_run_error}")
endif()
if(NOT positive_run_output MATCHES "CompiledChartTiming")
    message(FATAL_ERROR
        "VerifyChartCodecConsumer: positive consumer did not report CompiledChartTiming: "
        "${positive_run_output}")
endif()
cuexis_report("")

# Negative: each tools-only header is included by a dedicated translation unit and the build must
# fail at that include, for the expected header rather than for an unrelated reason.
set(negative_cases
    cxt2_loader|cxt_v2_loader.hpp
    candidate_lowering|candidate_lowering.hpp
)
foreach(negative_case IN LISTS negative_cases)
    string(REPLACE "|" ";" negative_parts "${negative_case}")
    list(GET negative_parts 0 negative_name)
    list(GET negative_parts 1 negative_header)
    set(negative_binary_dir "${CUEXIS_CONSUMER_WORK_ROOT}/negative-${negative_name}")
    cuexis_run_checked("negative ${negative_name} configure"
        "${CMAKE_COMMAND}" -S "${consumer_source}" -B "${negative_binary_dir}"
        "-DCUEXIS_CHART_CODEC_CONSUMER_NEGATIVE=${negative_name}" ${configure_arguments})
    execute_process(COMMAND "${CMAKE_COMMAND}" --build "${negative_binary_dir}"
            --target "rr_chart_codec_negative_${negative_name}"
        RESULT_VARIABLE negative_build_result
        OUTPUT_VARIABLE negative_build_output
        ERROR_VARIABLE negative_build_error)
    set(negative_build_log "${negative_build_output}\n${negative_build_error}")
    if(negative_build_result EQUAL 0)
        message(FATAL_ERROR
            "VerifyChartCodecConsumer: negative ${negative_name} unexpectedly compiled; the tools "
            "include root is visible to the pure codec consumer\n${negative_build_log}")
    endif()
    if(NOT negative_build_log MATCHES "${negative_header}")
        message(FATAL_ERROR
            "VerifyChartCodecConsumer: negative ${negative_name} failed without reporting "
            "'${negative_header}'\n${negative_build_log}")
    endif()
    string(REGEX MATCHALL "[^\r\n]*${negative_header}[^\r\n]*" negative_evidence
        "${negative_build_log}")
    string(REPLACE ";" " | " negative_evidence "${negative_evidence}")
    string(CONCAT negative_report_line
        "negative ${negative_name}: configure exit=0, build exit=${negative_build_result}, "
        "failure names ${negative_header}: ${negative_evidence}")
    cuexis_report("${negative_report_line}")
endforeach()

cuexis_report("")
cuexis_report("result: passed")
message(STATUS
    "VerifyChartCodecConsumer: positive compiled and ran, both tools headers failed to compile "
    "(work root '${CUEXIS_CONSUMER_WORK_ROOT}')")
