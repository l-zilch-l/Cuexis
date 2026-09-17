add_executable(cuexis_chart_tests
    animation_template_loader_tests.cpp
    canonical_chart_loader_tests.cpp
    canonical_semantic_chart_tests.cpp
    cxt_v2_loader_tests.cpp
    chart_foundation_capacity_tests.cpp
    chart_foundation_hardening_characterization_tests.cpp
    cfu_f4_limits_tests.cpp
    chart_loader_tests.cpp
    chart_migrator_tests.cpp
    chart_runtime_tests.cpp
    chart_v4_loader_tests.cpp
    chart_v4_c2_contract_tests.cpp
    chart_v4_resolver_tests.cpp
    chart_writer_tests.cpp
    example_chart_tests.cpp
    limits_tests.cpp
    parse_characterization_tests.cpp
    packed_budget_tests.cpp
    packed_atomic_write_tests.cpp
    packed_chart_primitives_tests.cpp
    packed_chart_io_tests.cpp
    packed_fixture_support.cpp
    packed_fixture_support.hpp
    packed_profile_rejection_tests.cpp
    packed_roundtrip_tests.cpp
    packed_semantic_identity_tests.cpp
    prepared_semantic_identity_tests.cpp
    rational_beat_tests.cpp
    timing_map_tests.cpp
    behavior_track_tests.cpp
    uuid_tests.cpp
)

target_compile_definitions(cuexis_chart_tests
    PRIVATE
        CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

target_include_directories(cuexis_chart_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/engine/json_support/src
)

target_link_libraries(cuexis_chart_tests
    PRIVATE
        cuexis::chart
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_chart_tests)

# R4 capacity probe: machine-readable Packed capacity data behind CUEXIS_RUN_PERFORMANCE_PROBE.
add_executable(cuexis_chart_capacity_probe
    chart_capacity_probe.cpp
)

target_compile_definitions(cuexis_chart_capacity_probe
    PRIVATE
        CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

target_link_libraries(cuexis_chart_capacity_probe
    PRIVATE
        cuexis::chart
)

if(WIN32)
    target_link_libraries(cuexis_chart_capacity_probe PRIVATE Psapi)
endif()

add_test(
    NAME cuexis_chart_capacity
    COMMAND "${CMAKE_COMMAND}"
        "-DCUEXIS_CHART_CAPACITY_PROBE=$<TARGET_FILE:cuexis_chart_capacity_probe>"
        "-DCUEXIS_CHART_CAPACITY_OUTPUT=${CMAKE_BINARY_DIR}/chart-capacity/capacity.json"
        "-DCUEXIS_IMPLEMENTATION_SHA=${CUEXIS_IMPLEMENTATION_SHA}"
        "-DCUEXIS_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${PROJECT_SOURCE_DIR}/cmake/VerifyChartCapacity.cmake"
)

set_tests_properties(cuexis_chart_capacity PROPERTIES
    LABELS "chart;packed;capacity;performance"
    TIMEOUT 900
)
