add_executable(cuexis_animation_tests
    animation_compiler_tests.cpp
    animation_mixer_tests.cpp
    animation_sampler_tests.cpp
)

target_link_libraries(cuexis_animation_tests
    PRIVATE
        cuexis::animation
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_animation_tests)
