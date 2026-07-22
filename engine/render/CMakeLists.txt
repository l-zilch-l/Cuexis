add_library(cuexis_render STATIC)
add_library(cuexis::render ALIAS cuexis_render)

target_sources(cuexis_render
    PRIVATE
        src/render_scene.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/cuexis/render/render_backend.hpp
            include/cuexis/render/render_scene.hpp
            include/cuexis/render/renderable_component.hpp
)

target_compile_features(cuexis_render PUBLIC cxx_std_20)
target_link_libraries(cuexis_render
    PUBLIC
        cuexis::assets
        cuexis::core
)
