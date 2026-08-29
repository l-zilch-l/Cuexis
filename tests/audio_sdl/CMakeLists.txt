add_executable(cuexis_audio_sdl_tests
    sdl_audio_tests.cpp
    wav_decoder_tests.cpp
)
target_compile_features(cuexis_audio_sdl_tests PRIVATE cxx_std_20)
target_compile_definitions(cuexis_audio_sdl_tests
    PRIVATE
        CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_link_libraries(cuexis_audio_sdl_tests
    PRIVATE cuexis::audio cuexis::audio_sdl Catch2::Catch2WithMain
)
include(Catch)
catch_discover_tests(cuexis_audio_sdl_tests
    PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy"
)
