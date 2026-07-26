add_library(cuexis_playback STATIC
    src/playback_session.cpp
)
add_library(cuexis::playback ALIAS cuexis_playback)
add_library(Cuexis::Playback ALIAS cuexis_playback)

set_target_properties(cuexis_playback PROPERTIES EXPORT_NAME Playback)

target_sources(cuexis_playback
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/cuexis/playback/content_provider.hpp
            include/cuexis/playback/playback_session.hpp
)

target_compile_features(cuexis_playback PUBLIC cxx_std_20)

target_link_libraries(cuexis_playback
    PUBLIC
        cuexis::assets
        cuexis::content
        cuexis::core
    PRIVATE
        cuexis::chart
        cuexis::runtime
        cuexis::render
        cuexis::world
)
