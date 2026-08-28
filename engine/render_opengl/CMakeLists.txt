find_package(glad CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)

add_library(
    cuexis_render_opengl
    STATIC
        src/open_gl_backend.cpp
        src/open_gl_presentation.cpp
        src/open_gl_presentation_internal.hpp
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
        cuexis::playback
    PRIVATE
        cuexis::shader_cache
        glad::glad
        SDL3::SDL3
)

if(TARGET cuexis_shader)
    target_link_libraries(cuexis_render_opengl PRIVATE cuexis::shader)
    target_compile_definitions(cuexis_render_opengl PUBLIC CUEXIS_HAS_SHADER_TOOLS)
endif()
