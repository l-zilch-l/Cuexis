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
    execution_kernel_oracle_tests.cpp
    rr_f132_identity_baseline_tests.cpp
    execution_kernel_phase_tests.cpp
    execution_kernel_test_fixture.hpp
    execution_reference.cpp
    gameplay_assembler_budget_tests.cpp
    gameplay_assembler_entry_tests.cpp
    gameplay_assembler_test_fixture.hpp
    gameplay_graph_tests.cpp
    gameplay_identity_tests.cpp
    gameplay_measure_compile_tests.cpp
    gameplay_pattern_budget_tests.cpp
    gameplay_pattern_compile_fixture.hpp
    gameplay_pattern_primitive_tests.cpp
    gameplay_prepare_tests.cpp
    input_mapping_tests.cpp
    judgement_boundary_tests.cpp
    judgement_diagnostic_tests.cpp
    judgement_session_tests.cpp
    observation_normalization_tests.cpp
    timebase_mapping_tests.cpp
    timebase_policy_tests.cpp
    timebase_test_fixture.hpp
    prepared_identity_self_contained.cpp
    phase_types_self_contained.cpp
    normalized_score_self_contained.cpp
    advance_self_contained.cpp
    prepared_gameplay_records_self_contained.cpp
    prepared_gameplay_self_contained.cpp
    legacy_prepared_view_tests.cpp
    runtime_identity_tests.cpp
    advance_kernel_self_contained.cpp
    identity_bytes_self_contained.cpp
    prepared_identity_bytes_self_contained.cpp
)

target_include_directories(cuexis_judgement_tests PRIVATE ${PROJECT_SOURCE_DIR}/engine/judgement/internal)
target_include_directories(cuexis_judgement_tests PRIVATE ${PROJECT_SOURCE_DIR}/engine/judgement/src)

# The candidate contract test exercises the build-only Chart v5 timing source and the author
# compiler; the default build does not compile either of them.
if(CUEXIS_ENABLE_CHART_V5_CANDIDATE)
    target_sources(cuexis_judgement_tests PRIVATE rr_timing_contract_tests.cpp)
    #  The v5 source values also expose the opaque JSON value they carry, which the Chart tools
    #  target keeps as a private dependency; the test names the directly used edge.
    target_link_libraries(cuexis_judgement_tests PRIVATE cuexis::chart_tools cuexis::json_support)
endif()

target_compile_features(cuexis_judgement_tests PRIVATE cxx_std_20)
target_link_libraries(cuexis_judgement_tests
    PRIVATE
        cuexis::judgement
        cuexis::judgement_compile
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_judgement_tests PROPERTIES LABELS "judgement")
