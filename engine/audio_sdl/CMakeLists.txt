find_package(SDL3 CONFIG REQUIRED)

add_library(cuexis_audio_sdl ${CUEXIS_LIBRARY_TYPE}
    src/sdl_audio.cpp
    src/wav_decoder.cpp
)
add_library(cuexis::audio_sdl ALIAS cuexis_audio_sdl)
add_library(Cuexis::AudioSDL ALIAS cuexis_audio_sdl)

generate_export_header(cuexis_audio_sdl
    BASE_NAME CUEXIS_AUDIO_SDL
    EXPORT_MACRO_NAME CUEXIS_AUDIO_SDL_API
    EXPORT_FILE_NAME "${PROJECT_BINARY_DIR}/generated/cuexis/audio_sdl/audio_sdl_export.hpp"
)
if(CUEXIS_LIBRARY_TYPE STREQUAL "STATIC")
    target_compile_definitions(cuexis_audio_sdl PUBLIC CUEXIS_AUDIO_SDL_STATIC_DEFINE)
else()
    target_compile_definitions(cuexis_audio_sdl PRIVATE CUEXIS_AUDIO_SDL_EXPORTS)
endif()

set_target_properties(cuexis_audio_sdl PROPERTIES EXPORT_NAME AudioSDL)
cuexis_configure_public_library(cuexis_audio_sdl cuexis_audio_sdl)

target_sources(cuexis_audio_sdl
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include "${PROJECT_BINARY_DIR}/generated"
        FILES
            "${PROJECT_BINARY_DIR}/generated/cuexis/audio_sdl/audio_sdl_export.hpp"
            include/cuexis/audio_sdl/sdl_audio.hpp
            include/cuexis/audio_sdl/wav_decoder.hpp
)

target_compile_features(cuexis_audio_sdl PUBLIC cxx_std_20)
target_link_libraries(cuexis_audio_sdl
    PUBLIC cuexis::audio
    PRIVATE SDL3::SDL3
)

if(CUEXIS_LIBRARY_TYPE STREQUAL "SHARED" AND WIN32)
    install(FILES "$<TARGET_FILE:SDL3::SDL3>"
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
endif()
