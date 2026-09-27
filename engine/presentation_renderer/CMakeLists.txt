add_library(cuexis_presentation_renderer STATIC
    src/draw_command.cpp
    src/presentation_renderer.cpp
)
add_library(cuexis::presentation_renderer ALIAS cuexis_presentation_renderer)

target_include_directories(cuexis_presentation_renderer
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(cuexis_presentation_renderer PUBLIC cxx_std_20)
target_link_libraries(cuexis_presentation_renderer
    PUBLIC
        cuexis::core
        cuexis::playback
        cuexis::render
)
