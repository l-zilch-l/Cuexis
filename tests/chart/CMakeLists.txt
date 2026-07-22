add_executable(cuexis_chart_tests
    canonical_chart_loader_tests.cpp
    chart_loader_tests.cpp
    chart_runtime_tests.cpp
    example_chart_tests.cpp
    limits_tests.cpp
    rational_beat_tests.cpp
    simple_chart_importer_tests.cpp
    timing_map_tests.cpp
    behavior_track_tests.cpp
    uuid_tests.cpp
)

target_compile_definitions(cuexis_chart_tests
    PRIVATE
        CUEXIS_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(cuexis_chart_tests
    PRIVATE
        cuexis::chart
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_chart_tests)
