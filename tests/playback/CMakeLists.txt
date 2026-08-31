add_executable(cuexis_playback_tests
    frame_digest_tests.cpp
    mode_parity_tests.cpp
    playback_identity_tests.cpp
    playback_migration_equivalence_tests.cpp
    playback_session_tests.cpp
    playback_source_tests.cpp
    preparation_characterization_tests.cpp
    playback_v4_animation_tests.cpp
    playback_v4_prepare_tests.cpp
    presentation_shader_budget_tests.cpp
    presentation_shader_tests.cpp
    presentation_tests.cpp
    runtime_timeline_tests.cpp
    ../presentation/validation_sink.cpp
    ${PROJECT_SOURCE_DIR}/engine/playback/src/frame_digest_version.cpp
    ${PROJECT_SOURCE_DIR}/engine/playback/src/presentation_extraction.cpp
)

target_compile_definitions(cuexis_playback_tests
    PRIVATE
        CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

target_include_directories(cuexis_playback_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/engine/assets/include
        ${PROJECT_SOURCE_DIR}/engine/json_support/src
        ${PROJECT_SOURCE_DIR}/engine/playback/src
        ${PROJECT_SOURCE_DIR}/tests/presentation
)

target_link_libraries(cuexis_playback_tests
    PRIVATE
        cuexis::audio
        cuexis::chart
        cuexis::playback
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_playback_tests)

add_executable(cuexis_playback_allocation_tests
    ../presentation/validation_sink.cpp
    playback_allocation_tests.cpp
    ${PROJECT_SOURCE_DIR}/engine/playback/src/presentation_extraction.cpp
)

target_compile_definitions(cuexis_playback_allocation_tests
    PRIVATE
        CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

if(CUEXIS_LIBRARY_TYPE STREQUAL "SHARED")
    target_compile_definitions(cuexis_playback_tests
        PRIVATE
            CUEXIS_PLAYBACK_PARSE_COUNT_PROBE_UNAVAILABLE=1
    )

    target_compile_definitions(cuexis_playback_allocation_tests
        PRIVATE
            CUEXIS_PLAYBACK_ALLOCATION_TEST_SHARED=1
    )
endif()

target_link_libraries(cuexis_playback_allocation_tests
    PRIVATE
        cuexis::playback
        Catch2::Catch2WithMain
)

target_include_directories(cuexis_playback_allocation_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/engine/playback/src
)

catch_discover_tests(cuexis_playback_allocation_tests)

add_executable(stage3_performance_probe EXCLUDE_FROM_ALL
    ../presentation/validation_sink.cpp
    stage3_performance_probe.cpp
)

target_compile_definitions(stage3_performance_probe
    PRIVATE
        CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

target_link_libraries(stage3_performance_probe
    PRIVATE
        cuexis::playback
)

if(WIN32)
    target_link_libraries(stage3_performance_probe PRIVATE Psapi)
endif()

add_executable(cuexis_s5h_performance_probe
    s5h_performance_probe.cpp
)

target_compile_definitions(cuexis_s5h_performance_probe
    PRIVATE
        CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

target_link_libraries(cuexis_s5h_performance_probe
    PRIVATE
        cuexis::playback
)

if(WIN32)
    target_link_libraries(cuexis_s5h_performance_probe PRIVATE Psapi)
endif()

add_test(
    NAME cuexis_s5h_performance
    COMMAND "${CMAKE_COMMAND}"
        "-DCUEXIS_S5H_PERFORMANCE_PROBE=$<TARGET_FILE:cuexis_s5h_performance_probe>"
        "-DCUEXIS_S5H_OUTPUT=${CMAKE_BINARY_DIR}/s5-h/performance.txt"
        "-DCUEXIS_IMPLEMENTATION_SHA=${CUEXIS_IMPLEMENTATION_SHA}"
        "-DCUEXIS_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${PROJECT_SOURCE_DIR}/cmake/VerifyS5HPerformance.cmake"
)

set_tests_properties(cuexis_s5h_performance PROPERTIES
    LABELS "s5-h;playback;performance"
    TIMEOUT 300
)
