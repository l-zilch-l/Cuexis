add_executable(cuexis_playback_tests
    playback_session_tests.cpp
)

target_compile_definitions(cuexis_playback_tests
    PRIVATE
        CUEXIS_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(cuexis_playback_tests
    PRIVATE
        cuexis::playback
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_playback_tests)
