add_library(cuexis_render STATIC)
add_library(cuexis::render ALIAS cuexis_render)

set_target_properties(cuexis_render PROPERTIES EXPORT_NAME InternalRender)

target_sources(cuexis_render
    PRIVATE
        src/render_scene.cpp
    PRIVATE
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/cuexis/render/camera_component.hpp
            include/cuexis/render/render_backend.hpp
            include/cuexis/render/render_scene.hpp
            include/cuexis/render/renderable_component.hpp
)

target_include_directories(cuexis_render PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(cuexis_render PUBLIC cxx_std_20)
target_link_libraries(cuexis_render
    PUBLIC
        cuexis::assets
        cuexis::core
)
