# The cache module exists whenever OpenGL or shader tools is enabled. Cache tests therefore
# register with the cache library alone; the compiler-backed cases are added only when the
# optional shader compiler target exists, so a GL ON / tools OFF job never pulls shaderc in.
set(cuexis_shader_test_sources
    shader_cache_tests.cpp
)

if(TARGET cuexis_shader)
    list(APPEND cuexis_shader_test_sources shader_compiler_tests.cpp)
endif()

add_executable(cuexis_shader_tests ${cuexis_shader_test_sources})

target_link_libraries(cuexis_shader_tests
    PRIVATE
        cuexis::shader_cache
        Catch2::Catch2WithMain
)

if(TARGET cuexis_shader)
    target_link_libraries(cuexis_shader_tests PRIVATE cuexis::shader)
    target_compile_definitions(cuexis_shader_tests PRIVATE CUEXIS_SHADER_COMPILER_AVAILABLE=1)
endif()

include(Catch)
catch_discover_tests(cuexis_shader_tests)
