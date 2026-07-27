add_library(cuexis_audio STATIC
    src/audio_clip.cpp
    src/audio_config.cpp
    src/audio_transport.cpp
)
add_library(cuexis::audio ALIAS cuexis_audio)
add_library(Cuexis::Audio ALIAS cuexis_audio)

set_target_properties(cuexis_audio PROPERTIES EXPORT_NAME Audio)

target_sources(cuexis_audio
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/cuexis/audio/audio_clip.hpp
            include/cuexis/audio/audio_config.hpp
            include/cuexis/audio/audio_transport.hpp
)

target_compile_features(cuexis_audio PUBLIC cxx_std_20)
target_link_libraries(cuexis_audio PUBLIC cuexis::core)
