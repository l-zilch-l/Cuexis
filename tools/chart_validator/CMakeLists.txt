add_executable(cuexis_chart_validator main.cpp)

target_compile_features(cuexis_chart_validator PRIVATE cxx_std_20)
target_link_libraries(cuexis_chart_validator PRIVATE cuexis::chart)

if(CUEXIS_BUILD_TESTS)
    add_test(
        NAME cuexis_chart_tool_tests
        COMMAND "${CMAKE_COMMAND}"
            "-DCUEXIS_CHART_VALIDATOR=$<TARGET_FILE:cuexis_chart_validator>"
            "-DCUEXIS_CHART_MIGRATOR=$<TARGET_FILE:cuexis_chart_migrator>"
            "-DCUEXIS_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DCUEXIS_BINARY_DIR=${PROJECT_BINARY_DIR}"
            -P "${PROJECT_SOURCE_DIR}/cmake/VerifyChartTools.cmake"
    )
    set_tests_properties(cuexis_chart_tool_tests PROPERTIES LABELS "tools;stage2")
endif()
