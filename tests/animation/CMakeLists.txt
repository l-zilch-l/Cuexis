add_executable(cuexis_animation_tests
    animation_compiler_tests.cpp
    animation_mixer_tests.cpp
    animation_sampler_tests.cpp
)

target_link_libraries(cuexis_animation_tests
    PRIVATE
        cuexis::animation
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_animation_tests)

add_executable(cuexis_s4g_performance_probe
    animation_performance_probe.cpp
)

target_link_libraries(cuexis_s4g_performance_probe
    PRIVATE
        cuexis::animation
)

if(WIN32)
    target_link_libraries(cuexis_s4g_performance_probe PRIVATE Psapi)
endif()

add_test(
    NAME cuexis_s4g_performance
    COMMAND "${CMAKE_COMMAND}"
        "-DCUEXIS_S4G_PERFORMANCE_PROBE=$<TARGET_FILE:cuexis_s4g_performance_probe>"
        "-DCUEXIS_S4G_OUTPUT=${CMAKE_BINARY_DIR}/s4-g/performance.txt"
        "-DCUEXIS_IMPLEMENTATION_SHA=${CUEXIS_IMPLEMENTATION_SHA}"
        "-DCUEXIS_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${PROJECT_SOURCE_DIR}/cmake/VerifyS4GPerformance.cmake"
)

set_tests_properties(cuexis_s4g_performance PROPERTIES
    LABELS "s4-g;animation;performance"
    TIMEOUT 300
)
