if(TARGET cuexis_render_opengl)
    add_executable(cuexis_render_opengl_tests
        frame_bridge_tests.cpp
        frame_layout_tests.cpp
        open_gl_config_tests.cpp
        open_gl_presentation_tests.cpp
        ${PROJECT_SOURCE_DIR}/engine/render_opengl/src/frame_layout_test_support.cpp
    )

    target_link_libraries(cuexis_render_opengl_tests
        PRIVATE
            cuexis::playback
            cuexis::platform_sdl
            cuexis::render_opengl
            Catch2::Catch2WithMain
    )

    target_compile_definitions(cuexis_render_opengl_tests
        PRIVATE
            CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    )

    target_include_directories(cuexis_render_opengl_tests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/engine/chart/include
            ${PROJECT_SOURCE_DIR}/engine/playback/experimental_include
            ${PROJECT_SOURCE_DIR}/engine/playback/src
            ${PROJECT_SOURCE_DIR}/engine/render_opengl/src
            ${PROJECT_SOURCE_DIR}/engine/shader/include
            ${PROJECT_SOURCE_DIR}/tests/playback
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
