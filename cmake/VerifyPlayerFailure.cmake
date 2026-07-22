if(NOT DEFINED CUEXIS_PLAYER OR NOT EXISTS "${CUEXIS_PLAYER}")
    message(FATAL_ERROR "CUEXIS_PLAYER must name an existing executable")
endif()

if(CUEXIS_FAILURE_CASE STREQUAL "unknown_argument")
    execute_process(
        COMMAND "${CUEXIS_PLAYER}" --cuexis-invalid-option
        RESULT_VARIABLE result
        OUTPUT_VARIABLE standard_output
        ERROR_VARIABLE standard_error
    )
    set(expected_code "player.arguments.unknown")
elseif(CUEXIS_FAILURE_CASE STREQUAL "missing_chart_argument")
    execute_process(
        COMMAND "${CUEXIS_PLAYER}" --chart
        RESULT_VARIABLE result
        OUTPUT_VARIABLE standard_output
        ERROR_VARIABLE standard_error
    )
    set(expected_code "player.arguments.chart_path_missing")
elseif(CUEXIS_FAILURE_CASE STREQUAL "chart_open")
    execute_process(
        COMMAND "${CUEXIS_PLAYER}" --chart "${CUEXIS_PLAYER}.missing-chart"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE standard_output
        ERROR_VARIABLE standard_error
    )
    set(expected_code "player.chart.open_failed")
elseif(CUEXIS_FAILURE_CASE STREQUAL "missing_project_argument")
    execute_process(
        COMMAND "${CUEXIS_PLAYER}" --project
        RESULT_VARIABLE result
        OUTPUT_VARIABLE standard_output
        ERROR_VARIABLE standard_error
    )
    set(expected_code "player.arguments.project_path_missing")
elseif(CUEXIS_FAILURE_CASE STREQUAL "project_chart_conflict")
    execute_process(
        COMMAND "${CUEXIS_PLAYER}" --project ignored-project --chart ignored-chart
        RESULT_VARIABLE result
        OUTPUT_VARIABLE standard_output
        ERROR_VARIABLE standard_error
    )
    set(expected_code "player.arguments.project_chart_conflict")
elseif(CUEXIS_FAILURE_CASE STREQUAL "project_open")
    execute_process(
        COMMAND "${CUEXIS_PLAYER}" --project "${CUEXIS_PLAYER}.missing-project"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE standard_output
        ERROR_VARIABLE standard_error
    )
    set(expected_code "player.project.load_failed")
elseif(CUEXIS_FAILURE_CASE STREQUAL "sdl_init")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
            SDL_VIDEODRIVER=cuexis-invalid-driver
            "${CUEXIS_PLAYER}" --smoke-test
        RESULT_VARIABLE result
        OUTPUT_VARIABLE standard_output
        ERROR_VARIABLE standard_error
    )
    set(expected_code "platform.sdl.init_failed")
else()
    message(FATAL_ERROR "Unknown CUEXIS_FAILURE_CASE '${CUEXIS_FAILURE_CASE}'")
endif()

if(NOT result EQUAL 1)
    message(FATAL_ERROR
        "Expected Cuexis Player to exit with code 1, got ${result}\n"
        "stdout:\n${standard_output}\n"
        "stderr:\n${standard_error}"
    )
endif()

set(combined_output "${standard_output}\n${standard_error}")
if(NOT combined_output MATCHES "${expected_code}")
    message(FATAL_ERROR
        "Cuexis Player output did not contain '${expected_code}'\n${combined_output}"
    )
endif()
