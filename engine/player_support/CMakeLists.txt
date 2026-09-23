add_library(cuexis_player_support STATIC
    src/config_json.cpp
    src/config_location.cpp
    src/user_preferences.cpp
    src/audio_device_profile.cpp
    src/resolved_config.cpp
    src/player_command.cpp
)
add_library(cuexis::player_support ALIAS cuexis_player_support)

target_include_directories(cuexis_player_support
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(cuexis_player_support PUBLIC cxx_std_20)
target_link_libraries(cuexis_player_support
    PUBLIC
        cuexis::core
        cuexis::json_support
)
