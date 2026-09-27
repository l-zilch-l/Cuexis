add_executable(cuexis_player_support_tests
    player_support_tests.cpp
    player_command_tests.cpp
)

target_compile_features(cuexis_player_support_tests PRIVATE cxx_std_20)
target_compile_definitions(cuexis_player_support_tests
    PRIVATE
        CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_link_libraries(cuexis_player_support_tests
    PRIVATE
        cuexis::player_support
        Catch2::Catch2
)

include(Catch)
catch_discover_tests(cuexis_player_support_tests)
