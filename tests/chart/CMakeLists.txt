add_executable(cuexis_chart_tests
    animation_template_loader_tests.cpp
    canonical_chart_loader_tests.cpp
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

target_link_libraries(cuexis_chart_tests
    PRIVATE
        cuexis::chart
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_chart_tests)
