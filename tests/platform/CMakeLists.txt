add_executable(cuexis_platform_sdl_tests
    sdl_window_tests.cpp
)

target_link_libraries(cuexis_platform_sdl_tests
    PRIVATE
        cuexis::platform_sdl
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_platform_sdl_tests
    PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy"
)
