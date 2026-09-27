add_executable(cuexis_player_diagnostics_tests
    frame_diagnostics_tests.cpp
    "${PROJECT_SOURCE_DIR}/app/player/src/frame_diagnostics.cpp"
    "${PROJECT_SOURCE_DIR}/app/player/src/snapshot_scene.cpp"
)

target_compile_features(cuexis_player_diagnostics_tests PRIVATE cxx_std_20)
target_include_directories(cuexis_player_diagnostics_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/app/player/src"
)
target_link_libraries(cuexis_player_diagnostics_tests
    PRIVATE
        cuexis::audio
        cuexis::core
        cuexis::playback
        cuexis::render
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_player_diagnostics_tests)

# The application command layer without a GPU, a window, or an audio device. It compiles the
# control layer directly and drives it with the neutral TestPresentationRenderer plus the shared
# fake audio transport, so the transition table and the staged transaction failures are testable
# headlessly. The logging target is imported here because the Player directory scope owns it.
find_package(spdlog CONFIG REQUIRED)

add_executable(cuexis_player_control_tests
    player_control_tests.cpp
    "${PROJECT_SOURCE_DIR}/app/player/src/player_control.cpp"
    "${PROJECT_SOURCE_DIR}/app/player/src/player_log.cpp"
    "${PROJECT_SOURCE_DIR}/app/player/src/frame_diagnostics.cpp"
    "${PROJECT_SOURCE_DIR}/app/player/src/snapshot_scene.cpp"
)

target_compile_features(cuexis_player_control_tests PRIVATE cxx_std_20)
target_compile_definitions(cuexis_player_control_tests
    PRIVATE
        CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_include_directories(cuexis_player_control_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/app/player/src"
        "${PROJECT_SOURCE_DIR}/tests/audio"
)
target_link_libraries(cuexis_player_control_tests
    PRIVATE
        cuexis::audio
        cuexis::core
        cuexis::filesystem
        cuexis::playback
        cuexis::player_support
        cuexis::presentation_renderer
        cuexis::render
        spdlog::spdlog
        Catch2::Catch2WithMain
)

catch_discover_tests(cuexis_player_control_tests)
