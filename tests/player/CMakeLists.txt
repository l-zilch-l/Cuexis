add_executable(cuexis_player_diagnostics_tests
    frame_diagnostics_tests.cpp
    "${PROJECT_SOURCE_DIR}/app/player/src/frame_diagnostics.cpp"
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
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_player_diagnostics_tests)
