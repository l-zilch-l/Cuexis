add_executable(cuexis_judgement_tests
    claim_key_tests.cpp
    shared_sequence_tests.cpp
    fold_recovery_tests.cpp
    fold_recovery_seek_tests.cpp
    fold_recovery_diagnostic_tests.cpp
    fold_recovery_deadline_tests.cpp
    fold_recovery_fixture.hpp
    publication_tests.cpp
    execution_profile_tests.cpp
    execution_kernel_tests.cpp
    execution_reference.cpp
    gameplay_assembler_budget_tests.cpp
    gameplay_assembler_entry_tests.cpp
    gameplay_assembler_test_fixture.hpp
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

target_include_directories(cuexis_judgement_tests PRIVATE ${PROJECT_SOURCE_DIR}/engine/judgement/internal)
target_include_directories(cuexis_judgement_tests PRIVATE ${PROJECT_SOURCE_DIR}/engine/judgement/src)

target_compile_features(cuexis_judgement_tests PRIVATE cxx_std_20)
target_link_libraries(cuexis_judgement_tests
    PRIVATE
        cuexis::judgement
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_judgement_tests PROPERTIES LABELS "judgement")
