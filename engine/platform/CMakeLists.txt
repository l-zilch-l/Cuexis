find_package(SDL3 CONFIG REQUIRED)

add_library(cuexis_platform_sdl STATIC
    src/sdl_state.hpp
    src/sdl_runtime.cpp
    src/sdl_window.cpp
)
add_library(cuexis::platform_sdl ALIAS cuexis_platform_sdl)

target_compile_features(cuexis_platform_sdl PUBLIC cxx_std_20)

target_include_directories(cuexis_platform_sdl
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(cuexis_platform_sdl
    PUBLIC
        cuexis::core
    PRIVATE
        SDL3::SDL3
)
