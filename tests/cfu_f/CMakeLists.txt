add_executable(cuexis_cfu_f_headless_consumer
    headless_reference_consumer.cpp
)

target_compile_definitions(cuexis_cfu_f_headless_consumer
    PRIVATE
        CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

target_link_libraries(cuexis_cfu_f_headless_consumer
    PRIVATE
        cuexis::playback
)

add_test(
    NAME cuexis_cfu_f_headless_reference
    COMMAND cuexis_cfu_f_headless_consumer
)

set_tests_properties(cuexis_cfu_f_headless_reference PROPERTIES
    LABELS "cfu-f;cfu-f1;headless;playback;cxc"
)

add_executable(cuexis_cfu_f4_performance_probe
    cfu_f4_performance_probe.cpp
)

target_compile_definitions(cuexis_cfu_f4_performance_probe
    PRIVATE
        CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

target_link_libraries(cuexis_cfu_f4_performance_probe
    PRIVATE
        cuexis_cxc
        cuexis::playback
)

if(WIN32)
    target_link_libraries(cuexis_cfu_f4_performance_probe PRIVATE Psapi)
endif()

add_test(
    NAME cuexis_cfu_f4_performance
    COMMAND "${CMAKE_COMMAND}"
        "-DCUEXIS_CFU_F4_PERFORMANCE_PROBE=$<TARGET_FILE:cuexis_cfu_f4_performance_probe>"
        "-DCUEXIS_CFU_F4_OUTPUT=${CMAKE_BINARY_DIR}/cfu-f4/performance.txt"
        "-DCUEXIS_IMPLEMENTATION_SHA=${CUEXIS_IMPLEMENTATION_SHA}"
        "-DCUEXIS_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${PROJECT_SOURCE_DIR}/cmake/VerifyCfuF4Performance.cmake"
)

set_tests_properties(cuexis_cfu_f4_performance PROPERTIES
    LABELS "cfu-f;cfu-f4;performance;cxc;chart;playback"
    TIMEOUT 300
)
