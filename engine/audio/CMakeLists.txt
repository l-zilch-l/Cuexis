add_library(cuexis_audio INTERFACE)
add_library(cuexis::audio ALIAS cuexis_audio)
target_link_libraries(cuexis_audio INTERFACE cuexis::core)
