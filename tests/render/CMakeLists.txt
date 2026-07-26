if(TARGET cuexis_render_opengl)
    add_executable(cuexis_render_opengl_tests
        open_gl_config_tests.cpp
    )

    target_link_libraries(cuexis_render_opengl_tests
        PRIVATE
            cuexis::render_opengl
            Catch2::Catch2WithMain
    )

    include(Catch)
    catch_discover_tests(cuexis_render_opengl_tests
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy"
    )
endif()

add_executable(cuexis_render_tests
    render_scene_tests.cpp
)

target_compile_features(cuexis_render_tests PRIVATE cxx_std_20)
target_link_libraries(cuexis_render_tests
    PRIVATE
        cuexis::render
        Catch2::Catch2WithMain
)

catch_discover_tests(cuexis_render_tests)
