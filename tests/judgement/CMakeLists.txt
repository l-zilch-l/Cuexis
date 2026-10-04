add_executable(cuexis_judgement_tests
    gameplay_assembler_tests.cpp
    gameplay_graph_tests.cpp
    gameplay_identity_tests.cpp
    gameplay_measure_compile_tests.cpp
    gameplay_pattern_compile_tests.cpp
    gameplay_prepare_tests.cpp
    input_mapping_tests.cpp
    judgement_boundary_tests.cpp
    judgement_diagnostic_tests.cpp
    judgement_session_tests.cpp
    observation_normalization_tests.cpp
    timebase_tests.cpp
)

target_compile_features(cuexis_judgement_tests PRIVATE cxx_std_20)
target_link_libraries(cuexis_judgement_tests
    PRIVATE
        cuexis::judgement
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_judgement_tests)
