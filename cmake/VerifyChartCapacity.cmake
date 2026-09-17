cmake_minimum_required(VERSION 3.25)

foreach(required CUEXIS_CHART_CAPACITY_PROBE CUEXIS_CHART_CAPACITY_OUTPUT CUEXIS_SOURCE_DIR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

if(NOT "$ENV{CUEXIS_RUN_PERFORMANCE_PROBE}" MATCHES "^(1|ON|TRUE|YES)$")
    message(STATUS
        "Chart capacity probe skipped; set CUEXIS_RUN_PERFORMANCE_PROBE=1 to run it")
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
        message(FATAL_ERROR "Chart capacity evidence requires an implementation SHA")
    endif()
endif()
string(TOLOWER "${implementation_sha}" implementation_sha)
string(LENGTH "${implementation_sha}" implementation_sha_length)
if(NOT implementation_sha_length EQUAL 40 OR
   NOT implementation_sha MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "Chart capacity implementation SHA must be 40 hexadecimal characters")
endif()

execute_process(
    COMMAND "${CUEXIS_CHART_CAPACITY_PROBE}" --sha "${implementation_sha}"
    RESULT_VARIABLE probe_result
    OUTPUT_VARIABLE probe_output
    ERROR_VARIABLE probe_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT probe_result EQUAL 0)
    message(FATAL_ERROR
        "Chart capacity probe failed (${probe_result})\n${probe_output}\n${probe_error}")
endif()

# Required machine-readable fields for every measured profile, per plan R4.6. CMake's regex engine
# has no {n} repetition, so the 40-character SHA pattern is spelled out.
set(sha_pattern "")
foreach(index RANGE 1 40)
    string(APPEND sha_pattern "[0-9a-f]")
endforeach()
set(required_patterns
    "\"implementationSha\": \"${sha_pattern}\""
    "\"id\": \"low-reuse-v1\""
    "\"id\": \"cxt-stair-ladder\""
    "\"packedBytes\": [0-9]+"
    "\"decodedBytes\": [0-9]+"
    "\"directoryBytes\": [0-9]+"
    "\"type\": \"IDN0\""
    "\"residentBefore\": [0-9]+"
    "\"peakDelta\": [0-9]+"
    "\"encode\": [0-9]+([.][0-9]+)?"
    "\"decode\": [0-9]+([.][0-9]+)?"
)
foreach(pattern IN LISTS required_patterns)
    if(NOT probe_output MATCHES "${pattern}")
        message(FATAL_ERROR "Chart capacity output is missing required pattern: ${pattern}")
    endif()
endforeach()

get_filename_component(output_directory "${CUEXIS_CHART_CAPACITY_OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")
file(WRITE "${CUEXIS_CHART_CAPACITY_OUTPUT}" "${probe_output}\n")
message(STATUS "Chart capacity evidence: ${CUEXIS_CHART_CAPACITY_OUTPUT}")
