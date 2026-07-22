add_library(cuexis_audio_sdl INTERFACE)
add_library(cuexis::audio_sdl ALIAS cuexis_audio_sdl)
target_link_libraries(cuexis_audio_sdl INTERFACE cuexis::audio)
