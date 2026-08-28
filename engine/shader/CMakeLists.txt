# Shader cache format is available whenever OpenGL or shader-tools is enabled.
# cuexis_shader (compiler + worker pipeline cache) remains shader-tools-only.

if(CUEXIS_BUILD_SHADER_TOOLS)
    # Optional shader-tools feature. Imported target names are recorded here and in
    # docs/guides/DEPENDENCY_POLICY.md; they must not enter CuexisConfig.cmake.
    find_package(unofficial-shaderc CONFIG REQUIRED)
    find_package(spirv_cross_core CONFIG REQUIRED)
    find_package(spirv_cross_glsl CONFIG REQUIRED)
    find_package(spirv_cross_reflect CONFIG REQUIRED)
    find_package(SPIRV-Tools CONFIG REQUIRED)

    message(STATUS
        "Cuexis shader tools: unofficial::shaderc::shaderc, "
        "spirv-cross-core, spirv-cross-glsl, spirv-cross-reflect, SPIRV-Tools-static")
endif()

if("${unofficial-shaderc_VERSION}" STREQUAL "")
    set(unofficial-shaderc_VERSION "2026.2")
endif()
if("${glslang_VERSION}" STREQUAL "")
    set(glslang_VERSION "16.4.0")
endif()
if("${SPIRV-Tools_VERSION}" STREQUAL "")
    set(SPIRV-Tools_VERSION "1.4.350.1")
endif()
if("${spirv_cross_core_VERSION}" STREQUAL "")
    set(spirv_cross_core_VERSION "1.4.350.1")
endif()

add_library(cuexis_shader_cache STATIC
    src/shader_cache.cpp
    src/shader_reflection.cpp
)
add_library(cuexis::shader_cache ALIAS cuexis_shader_cache)

set_target_properties(cuexis_shader_cache PROPERTIES EXPORT_NAME InternalShaderCache)

target_sources(cuexis_shader_cache
    PRIVATE
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/cuexis/shader/shader_cache.hpp
            include/cuexis/shader/shader_compiler.hpp
            include/cuexis/shader/shader_diagnostics.hpp
)

target_compile_definitions(cuexis_shader_cache
    PRIVATE
        CUEXIS_SHADERC_VERSION="${unofficial-shaderc_VERSION}"
        CUEXIS_GLSLANG_VERSION="${glslang_VERSION}"
        CUEXIS_SPIRV_TOOLS_VERSION="${SPIRV-Tools_VERSION}"
        CUEXIS_SPIRV_CROSS_VERSION="${spirv_cross_core_VERSION}"
)

target_include_directories(cuexis_shader_cache PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(cuexis_shader_cache PUBLIC cxx_std_20)

target_link_libraries(cuexis_shader_cache
    PUBLIC
        cuexis::core
)

if(CUEXIS_BUILD_SHADER_TOOLS)
    add_library(cuexis_shader STATIC
        src/shader_compiler.cpp
        src/shader_pipeline_cache.cpp
    )
    add_library(cuexis::shader ALIAS cuexis_shader)

    set_target_properties(cuexis_shader PROPERTIES EXPORT_NAME InternalShader)

    target_compile_definitions(cuexis_shader
        PRIVATE
            CUEXIS_SHADERC_VERSION="${unofficial-shaderc_VERSION}"
            CUEXIS_GLSLANG_VERSION="${glslang_VERSION}"
            CUEXIS_SPIRV_TOOLS_VERSION="${SPIRV-Tools_VERSION}"
            CUEXIS_SPIRV_CROSS_VERSION="${spirv_cross_core_VERSION}"
    )

    target_include_directories(cuexis_shader PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

    target_compile_features(cuexis_shader PUBLIC cxx_std_20)

    target_link_libraries(cuexis_shader
        PUBLIC
            cuexis::shader_cache
        PRIVATE
            unofficial::shaderc::shaderc
            spirv-cross-core
            spirv-cross-glsl
            spirv-cross-reflect
            SPIRV-Tools-static
    )
endif()
