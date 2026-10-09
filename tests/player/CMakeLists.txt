add_executable(cuexis_player_diagnostics_tests
    frame_diagnostics_tests.cpp
    "${PROJECT_SOURCE_DIR}/app/player/src/frame_diagnostics.cpp"
    "${PROJECT_SOURCE_DIR}/app/player/src/player_state_name.cpp"
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
    player_candidate_options_tests.cpp
    "${PROJECT_SOURCE_DIR}/app/player/src/player_options.cpp"
    "${PROJECT_SOURCE_DIR}/app/player/src/player_control.cpp"
    "${PROJECT_SOURCE_DIR}/app/player/src/player_gameplay.cpp"
    "${PROJECT_SOURCE_DIR}/app/player/src/player_gameplay_guide.cpp"
    "${PROJECT_SOURCE_DIR}/app/player/src/player_log.cpp"
    "${PROJECT_SOURCE_DIR}/app/player/src/frame_diagnostics.cpp"
    "${PROJECT_SOURCE_DIR}/app/player/src/player_state_name.cpp"
    "${PROJECT_SOURCE_DIR}/app/player/src/snapshot_scene.cpp"
)

target_compile_features(cuexis_player_control_tests PRIVATE cxx_std_20)
target_compile_definitions(cuexis_player_control_tests
    PRIVATE
        CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        CUEXIS_BUILD_DIR="${PROJECT_BINARY_DIR}"
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

if(CUEXIS_ENABLE_CHART_V5_CANDIDATE)
    target_sources(cuexis_player_control_tests PRIVATE player_gameplay_tests.cpp)
    add_test(NAME cuexis_gameplay_player_fixture COMMAND cuexis_playback_tests "[.candidate-gameplay-fixture]")
    set_tests_properties(cuexis_gameplay_player_fixture PROPERTIES
        FIXTURES_SETUP cuexis_gameplay_player_fixture LABELS "candidate;gameplay;player;headless"
        RESOURCE_LOCK cuexis_external_consumer)
    catch_discover_tests(cuexis_player_control_tests PROPERTIES FIXTURES_REQUIRED cuexis_gameplay_player_fixture)
else()
    catch_discover_tests(cuexis_player_control_tests)
endif()
