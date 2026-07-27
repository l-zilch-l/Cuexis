find_package(SDL3 CONFIG REQUIRED)

add_library(cuexis_audio_sdl STATIC
    src/sdl_audio.cpp
    src/wav_decoder.cpp
)
add_library(cuexis::audio_sdl ALIAS cuexis_audio_sdl)
add_library(Cuexis::AudioSDL ALIAS cuexis_audio_sdl)

set_target_properties(cuexis_audio_sdl PROPERTIES EXPORT_NAME AudioSDL)

target_sources(cuexis_audio_sdl
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/cuexis/audio_sdl/sdl_audio.hpp
            include/cuexis/audio_sdl/wav_decoder.hpp
)

target_compile_features(cuexis_audio_sdl PUBLIC cxx_std_20)
target_link_libraries(cuexis_audio_sdl
    PUBLIC cuexis::audio
    PRIVATE SDL3::SDL3
)
