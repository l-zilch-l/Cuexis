if(NOT DEFINED CUEXIS_CHART_VALIDATOR OR NOT DEFINED CUEXIS_CHART_MIGRATOR OR
   NOT DEFINED CUEXIS_SOURCE_DIR OR NOT DEFINED CUEXIS_BINARY_DIR)
    message(FATAL_ERROR "Chart tool verification requires tool, source, and binary paths")
endif()

set(valid_chart "${CUEXIS_SOURCE_DIR}/assets/charts/stage2_example.cuexis.chart.json")
set(source_chart "${CUEXIS_SOURCE_DIR}/tests/fixtures/stage2_migration_v1.cuexis.chart.json")
set(golden_chart
    "${CUEXIS_SOURCE_DIR}/tests/fixtures/stage2_migration_v3.golden.cuexis.chart.json")
set(golden_report "${CUEXIS_SOURCE_DIR}/tests/fixtures/stage2_migration_report.golden.json")
set(tool_dir "${CUEXIS_BINARY_DIR}/chart-tool-test")
set(output_chart "${tool_dir}/migrated.cuexis.chart.json")
set(output_report "${tool_dir}/migration-report.json")
file(REMOVE_RECURSE "${tool_dir}")
file(MAKE_DIRECTORY "${tool_dir}")

execute_process(
    COMMAND "${CUEXIS_CHART_VALIDATOR}" --input "${valid_chart}"
    RESULT_VARIABLE validator_result
    OUTPUT_VARIABLE validator_output
    ERROR_VARIABLE validator_error
)
if(NOT validator_result EQUAL 0)
    message(FATAL_ERROR
        "Chart validator rejected the Stage 2 fixture:\n${validator_output}${validator_error}")
endif()

execute_process(
    COMMAND "${CUEXIS_CHART_MIGRATOR}"
        --input "${source_chart}"
        --output "${output_chart}"
        --report "${output_report}"
    RESULT_VARIABLE migrator_result
    OUTPUT_VARIABLE migrator_output
    ERROR_VARIABLE migrator_error
)
if(NOT migrator_result EQUAL 0)
    message(FATAL_ERROR
        "Chart migrator rejected the migration fixture:\n${migrator_output}${migrator_error}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${output_chart}" "${golden_chart}"
                RESULT_VARIABLE chart_compare_result)
execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${output_report}" "${golden_report}"
                RESULT_VARIABLE report_compare_result)
if(NOT chart_compare_result EQUAL 0 OR NOT report_compare_result EQUAL 0)
    message(FATAL_ERROR "Chart migration CLI output differs from the committed golden files")
endif()

file(WRITE "${output_chart}" "preserve chart\n")
file(WRITE "${output_report}" "preserve report\n")
execute_process(
    COMMAND "${CUEXIS_CHART_MIGRATOR}"
        --input "${valid_chart}"
        --output "${output_chart}"
        --report "${output_report}"
    RESULT_VARIABLE failure_result
)
if(failure_result EQUAL 0)
    message(FATAL_ERROR "Chart migrator unexpectedly accepted a v3 source")
endif()
file(READ "${output_chart}" preserved_chart)
file(READ "${output_report}" preserved_report)
if(NOT preserved_chart STREQUAL "preserve chart\n" OR
   NOT preserved_report STREQUAL "preserve report\n")
    message(FATAL_ERROR "Failed migration modified an output target")
endif()

execute_process(
    COMMAND "${CUEXIS_CHART_MIGRATOR}"
        --input "${source_chart}"
        --output "${source_chart}"
        --report "${output_report}"
    RESULT_VARIABLE conflict_result
)
if(conflict_result EQUAL 0)
    message(FATAL_ERROR "Chart migrator accepted an output path equal to the source path")
endif()
