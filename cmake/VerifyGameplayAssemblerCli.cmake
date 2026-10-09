cmake_minimum_required(VERSION 3.25)
foreach(required IN ITEMS CUEXIS_SOURCE_DIR CUEXIS_BINARY_DIR CUEXIS_PLAYBACK_TESTS CUEXIS_GAMEPLAY_TOOL)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
find_program(gameplay_python NAMES python3 python REQUIRED)
execute_process(COMMAND "${CUEXIS_PLAYBACK_TESTS}" "[.candidate-gameplay-fixture]"
    RESULT_VARIABLE fixture_status OUTPUT_VARIABLE fixture_output ERROR_VARIABLE fixture_errors)
if(NOT fixture_status EQUAL 0)
    message(FATAL_ERROR "Explicit typed fixture failed:\n${fixture_output}\n${fixture_errors}")
endif()
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef nonce)
set(log_directory "${CUEXIS_BINARY_DIR}/s7a78-cli-tests/${nonce}")
execute_process(COMMAND "${gameplay_python}" -B
    "${CUEXIS_SOURCE_DIR}/tools/check_gameplay_assembler_tests.py"
    --tool "${CUEXIS_GAMEPLAY_TOOL}" --dispatch
    --fixture "${CUEXIS_BINARY_DIR}/s7a78-public-fixture" --log-dir "${log_directory}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "Actual existing Gameplay CLI gate failed:\n${output}\n${errors}\nlogs=${log_directory}")
endif()
message(STATUS "${output}; raw CLI logs=${log_directory}")
