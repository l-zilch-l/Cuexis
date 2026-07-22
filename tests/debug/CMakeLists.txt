add_executable(cuexis_debug_tests
    debug_draw_tests.cpp
)

target_compile_features(cuexis_debug_tests PRIVATE cxx_std_20)
target_link_libraries(cuexis_debug_tests
    PRIVATE
        cuexis::debug
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_debug_tests)
