add_executable(cuexis_shader_tests
    shader_compiler_tests.cpp
    shader_cache_tests.cpp
)

target_link_libraries(cuexis_shader_tests
    PRIVATE
        cuexis::shader
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_shader_tests)
