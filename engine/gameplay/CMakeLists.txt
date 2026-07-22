add_library(cuexis_gameplay INTERFACE)
add_library(cuexis::gameplay ALIAS cuexis_gameplay)

target_compile_features(cuexis_gameplay INTERFACE cxx_std_20)

target_include_directories(cuexis_gameplay
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(cuexis_gameplay INTERFACE cuexis::core)
