add_library(cuexis_playback STATIC
    src/playback_session.cpp
)
add_library(cuexis::playback ALIAS cuexis_playback)

target_compile_features(cuexis_playback PUBLIC cxx_std_20)

target_include_directories(cuexis_playback
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(cuexis_playback
    PUBLIC
        cuexis::core
    PRIVATE
        cuexis::chart
        cuexis::runtime
        cuexis::assets
        cuexis::render
        cuexis::world
)