add_library(cuexis_audio ${CUEXIS_LIBRARY_TYPE}
    src/audio_clip.cpp
    src/audio_config.cpp
    src/audio_transport.cpp
)
add_library(cuexis::audio ALIAS cuexis_audio)
add_library(Cuexis::Audio ALIAS cuexis_audio)

generate_export_header(cuexis_audio
    BASE_NAME CUEXIS_AUDIO
    EXPORT_MACRO_NAME CUEXIS_AUDIO_API
    EXPORT_FILE_NAME "${PROJECT_BINARY_DIR}/generated/cuexis/audio/audio_export.hpp"
)
if(CUEXIS_LIBRARY_TYPE STREQUAL "STATIC")
    target_compile_definitions(cuexis_audio PUBLIC CUEXIS_AUDIO_STATIC_DEFINE)
else()
    target_compile_definitions(cuexis_audio PRIVATE CUEXIS_AUDIO_EXPORTS)
endif()

set_target_properties(cuexis_audio PROPERTIES EXPORT_NAME Audio)
cuexis_configure_public_library(cuexis_audio cuexis_audio)

target_sources(cuexis_audio
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include "${PROJECT_BINARY_DIR}/generated"
        FILES
            "${PROJECT_BINARY_DIR}/generated/cuexis/audio/audio_export.hpp"
            include/cuexis/audio/audio_clip.hpp
            include/cuexis/audio/audio_config.hpp
            include/cuexis/audio/audio_transport.hpp
)

target_compile_features(cuexis_audio PUBLIC cxx_std_20)
target_link_libraries(cuexis_audio PUBLIC cuexis::core)
