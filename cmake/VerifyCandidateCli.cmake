cmake_minimum_required(VERSION 3.25)

execute_process(COMMAND "${CUEXIS_CHART_TESTS}" "[candidate-cli-input]"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Canonical metadata fixture failed: ${output}${errors}")
endif()
set(work "${CUEXIS_BINARY_DIR}/candidate-cli")
set(fixture "${CUEXIS_SOURCE_DIR}/tests/fixtures/chart_format_foundation/valid")
set(common
    --base "${CUEXIS_SOURCE_DIR}/tests/fixtures/chart_format_update/golden/cxc_v1_v4_static.cxc"
    --metadata "${work}/metadata.packed"
    --binding intro-stair --module pattern.stair --export stair)
execute_process(COMMAND "${CUEXIS_CANDIDATE_TOOL}" ${common}
    --template "${fixture}/pattern_stair.cxt" --output "${work}/assembled.cxc"
    RESULT_VARIABLE result OUTPUT_VARIABLE report ERROR_VARIABLE errors)
if(NOT CUEXIS_EXPERIMENTAL)
    if(result EQUAL 0 OR NOT errors MATCHES "candidate.disabled")
        message(FATAL_ERROR "OFF tool must reject the explicit candidate consumer: ${report}${errors}")
    endif()
    return()
endif()
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Actual candidate CLI failed: ${report}${errors}")
endif()
string(JSON requirement_count GET "${report}" requirements)
string(JSON semantic_identity GET "${report}" semanticIdentity)
if(NOT requirement_count EQUAL 16 OR
   NOT semantic_identity STREQUAL "c08e0e8a1236001058ae338cffac0cb609b8ee95fb23ceb5d7b0dbc3629febcc")
    message(FATAL_ERROR "Candidate CLI differs from the frozen 16-tap stair golden")
endif()
file(SHA256 "${work}/assembled.cxc" original_hash)
execute_process(COMMAND "${CUEXIS_CANDIDATE_TOOL}" ${common}
    --template "${fixture}/pattern_stair_reordered.cxt" --output "${work}/reordered.cxc"
    RESULT_VARIABLE result OUTPUT_VARIABLE reordered_report ERROR_VARIABLE errors)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Reordered CLI failed: ${errors}")
endif()
file(SHA256 "${work}/reordered.cxc" reordered_hash)
if(NOT original_hash STREQUAL reordered_hash OR NOT report STREQUAL reordered_report)
    message(FATAL_ERROR "Equivalent real author inputs produced different package/closure bytes")
endif()
# A failed invocation must preserve the previously published package and embedded report.
execute_process(COMMAND "${CUEXIS_CANDIDATE_TOOL}" ${common}
    --template "${work}/missing.cxt" --output "${work}/assembled.cxc"
    RESULT_VARIABLE result OUTPUT_VARIABLE rejected ERROR_VARIABLE errors)
file(SHA256 "${work}/assembled.cxc" after_hash)
if(result EQUAL 0 OR NOT after_hash STREQUAL original_hash)
    message(FATAL_ERROR "CLI failure changed the published package")
endif()
file(WRITE "${work}/closure-report.json" "${report}")
set(missing_resource_arguments ${common})
list(FIND missing_resource_arguments "${work}/metadata.packed" metadata_index)
list(REMOVE_AT missing_resource_arguments ${metadata_index})
list(INSERT missing_resource_arguments ${metadata_index} "${work}/missing-resource.packed")
execute_process(COMMAND "${CUEXIS_CANDIDATE_TOOL}" ${missing_resource_arguments}
    --template "${fixture}/pattern_stair.cxt" --output "${work}/assembled.cxc"
    RESULT_VARIABLE result OUTPUT_VARIABLE rejected ERROR_VARIABLE errors)
file(SHA256 "${work}/assembled.cxc" after_hash)
if(result EQUAL 0 OR NOT after_hash STREQUAL original_hash)
    message(FATAL_ERROR "Missing asset closure changed the published package")
endif()
message(STATUS "Actual typed CLI -> Packed -> CXC -> explicit memory entry -> Playback prepare passed; SHA256=${original_hash}")
