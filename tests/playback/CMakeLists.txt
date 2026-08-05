add_executable(cuexis_playback_tests
    mode_parity_tests.cpp
    playback_session_tests.cpp
    runtime_timeline_tests.cpp
)

target_compile_definitions(cuexis_playback_tests
    PRIVATE
        CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

target_link_libraries(cuexis_playback_tests
    PRIVATE
        cuexis::audio
        cuexis::playback
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_playback_tests)

add_executable(cuexis_playback_allocation_tests
    playback_allocation_tests.cpp
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

catch_discover_tests(cuexis_playback_allocation_tests)
