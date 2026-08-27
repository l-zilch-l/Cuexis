add_library(cuexis_playback ${CUEXIS_LIBRARY_TYPE}
    src/frame_digest.cpp
    src/frame_digest_internal.hpp
    src/frame_digest_version.cpp
    src/presentation.cpp
    src/presentation_extraction.cpp
    src/presentation_extraction.hpp
    src/presentation_internal.hpp
    src/playback_session.cpp
    src/playback_source.cpp
    src/playback_source_state.hpp
    src/runtime_timeline.cpp
)
add_library(cuexis::playback ALIAS cuexis_playback)
add_library(Cuexis::Playback ALIAS cuexis_playback)

generate_export_header(cuexis_playback
    BASE_NAME CUEXIS_PLAYBACK
    EXPORT_MACRO_NAME CUEXIS_PLAYBACK_API
    EXPORT_FILE_NAME "${PROJECT_BINARY_DIR}/generated/cuexis/playback/playback_export.hpp"
)
if(CUEXIS_LIBRARY_TYPE STREQUAL "STATIC")
    target_compile_definitions(cuexis_playback PUBLIC CUEXIS_PLAYBACK_STATIC_DEFINE)
else()
    target_compile_definitions(cuexis_playback PRIVATE CUEXIS_PLAYBACK_EXPORTS)
endif()

set_target_properties(cuexis_playback PROPERTIES EXPORT_NAME Playback)
cuexis_configure_public_library(cuexis_playback cuexis_playback)

target_sources(cuexis_playback
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include "${PROJECT_BINARY_DIR}/generated"
        FILES
            "${PROJECT_BINARY_DIR}/generated/cuexis/playback/playback_export.hpp"
            include/cuexis/playback/content_provider.hpp
            include/cuexis/playback/frame_digest.hpp
            include/cuexis/playback/playback_session.hpp
            include/cuexis/playback/playback_source.hpp
            include/cuexis/playback/presentation.hpp
            include/cuexis/playback/runtime_timeline.hpp
)

target_compile_features(cuexis_playback PUBLIC cxx_std_20)

target_link_libraries(cuexis_playback
    PUBLIC
        cuexis::audio
        cuexis::content
        cuexis::core
    PRIVATE
        cuexis::animation
        cuexis::assets
        cuexis::chart
        cuexis::cxc
        cuexis::filesystem
        cuexis::project
        cuexis::runtime
        cuexis::render
        cuexis::world
)
