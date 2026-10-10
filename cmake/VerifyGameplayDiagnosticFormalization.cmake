cmake_minimum_required(VERSION 3.25)
if(NOT DEFINED CUEXIS_SOURCE_DIR)
    message(FATAL_ERROR "CUEXIS_SOURCE_DIR is required")
endif()
find_program(cuexis_diagnostic_python NAMES python3 python REQUIRED)
execute_process(
    COMMAND "${cuexis_diagnostic_python}" -B
        "${CUEXIS_SOURCE_DIR}/tools/check_gameplay_diagnostic_formalization.py"
        --root "${CUEXIS_SOURCE_DIR}" ${CUEXIS_DIAGNOSTIC_EXTRA_ARGS}
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "Diagnostic formalization failed (${status}): ${output}${errors}")
endif()
message(STATUS "${output}")
