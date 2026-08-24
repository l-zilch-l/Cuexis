add_executable(cuexis_playback_tests
    frame_digest_tests.cpp
    mode_parity_tests.cpp
    playback_identity_tests.cpp
    playback_migration_equivalence_tests.cpp
    playback_session_tests.cpp
    playback_source_tests.cpp
    playback_v4_prepare_tests.cpp
    presentation_tests.cpp
    runtime_timeline_tests.cpp
    ${PROJECT_SOURCE_DIR}/engine/playback/src/frame_digest_version.cpp
    ${PROJECT_SOURCE_DIR}/engine/playback/src/presentation_extraction.cpp
)

target_compile_definitions(cuexis_playback_tests
    PRIVATE
        CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

target_include_directories(cuexis_playback_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/engine/playback/src
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
