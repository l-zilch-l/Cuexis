cmake_minimum_required(VERSION 3.25)

foreach(required CUEXIS_S4G_PERFORMANCE_PROBE CUEXIS_S4G_OUTPUT CUEXIS_SOURCE_DIR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

if(NOT "$ENV{CUEXIS_RUN_PERFORMANCE_PROBE}" MATCHES "^(1|ON|TRUE|YES)$")
    message(STATUS
        "S4-G performance probe skipped; set CUEXIS_RUN_PERFORMANCE_PROBE=1 to run it")
    return()
endif()

set(implementation_sha "${CUEXIS_IMPLEMENTATION_SHA}")
if(implementation_sha STREQUAL "")
    execute_process(
        COMMAND git -C "${CUEXIS_SOURCE_DIR}" rev-parse HEAD
        RESULT_VARIABLE git_result
        OUTPUT_VARIABLE implementation_sha
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT git_result EQUAL 0)
        message(FATAL_ERROR "S4-G performance evidence requires an implementation SHA")
    endif()
endif()
string(TOLOWER "${implementation_sha}" implementation_sha)
string(LENGTH "${implementation_sha}" implementation_sha_length)
if(NOT implementation_sha_length EQUAL 40 OR
   NOT implementation_sha MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "S4-G implementation SHA must be 40 hexadecimal characters")
endif()

execute_process(
    COMMAND "${CUEXIS_S4G_PERFORMANCE_PROBE}"
    RESULT_VARIABLE probe_result
    OUTPUT_VARIABLE probe_output
    ERROR_VARIABLE probe_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT probe_result EQUAL 0)
    message(FATAL_ERROR
        "S4-G performance probe failed (${probe_result})\n${probe_output}\n${probe_error}")
endif()

set(required_metrics
    clip_count
    object_count
    compile_us
    warmed_evaluate_avg_us
    resident_before_compile_bytes
    resident_after_compile_bytes
    peak_after_compile_bytes
    compile_resident_delta_bytes
    resident_after_warmup_bytes
    peak_after_warmup_bytes
    resident_after_hot_frames_bytes
    peak_after_hot_frames_bytes
    hot_peak_delta_bytes
)
foreach(metric IN LISTS required_metrics)
    if(NOT probe_output MATCHES "(^|\n)${metric}=([0-9]+([.][0-9]+)?)($|\n)")
        message(FATAL_ERROR "S4-G performance output is missing numeric ${metric}")
    endif()
endforeach()

get_filename_component(output_directory "${CUEXIS_S4G_OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")
file(WRITE "${CUEXIS_S4G_OUTPUT}"
    "implementation_sha=${implementation_sha}\n${probe_output}\n")
message(STATUS "S4-G performance evidence: ${CUEXIS_S4G_OUTPUT}")
