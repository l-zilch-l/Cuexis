add_executable(cuexis_timing_tests
    rational_beat_tests.cpp
    timing_map_tests.cpp
)

target_compile_features(cuexis_timing_tests PRIVATE cxx_std_20)
target_link_libraries(cuexis_timing_tests
    PRIVATE
        cuexis::timing
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_timing_tests PROPERTIES LABELS "timing")
