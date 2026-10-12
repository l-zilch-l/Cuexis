add_executable(cuexis_rpa_tests
    rpa_b1_fixture.cpp
    rpa_b1_tests.cpp
    rpa_b2_fixture.cpp
    rpa_b2_tests.cpp
)

target_compile_features(cuexis_rpa_tests PRIVATE cxx_std_20)
target_link_libraries(cuexis_rpa_tests
    PRIVATE
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_rpa_tests PROPERTIES LABELS "rpa;b1;b2")
