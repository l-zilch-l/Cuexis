find_package(glad CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)

add_library(
    cuexis_render_opengl
    STATIC
        src/open_gl_backend.cpp
)
add_library(cuexis::render_opengl ALIAS cuexis_render_opengl)

target_include_directories(
    cuexis_render_opengl
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(cuexis_render_opengl PUBLIC cxx_std_20)

target_link_libraries(
    cuexis_render_opengl
    PUBLIC
        cuexis::render
        cuexis::platform_sdl
    PRIVATE
        glad::glad
        SDL3::SDL3
)
