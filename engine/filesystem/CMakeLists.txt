add_library(cuexis_filesystem STATIC
    src/secure_file.cpp
)
add_library(cuexis::filesystem ALIAS cuexis_filesystem)

target_compile_features(cuexis_filesystem PUBLIC cxx_std_20)

target_include_directories(cuexis_filesystem
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(cuexis_filesystem PUBLIC cuexis::core)
