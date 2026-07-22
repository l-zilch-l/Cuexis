cmake_minimum_required(VERSION 3.25)

include(cmake/CuexisVersion.cmake)

project(Cuexis VERSION ${CUEXIS_CMAKE_VERSION} LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

configure_file(
    cmake/version.hpp.in
    "${CMAKE_BINARY_DIR}/generated/cuexis/version.hpp"
    @ONLY
)

include(CTest)
include(cmake/CuexisWarnings.cmake)
include(cmake/VerifyArchitecture.cmake)

add_subdirectory(engine)
add_subdirectory(app)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

target_include_directories(cuexis_core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>"
)

set(CUEXIS_ACTIVE_TARGETS
    cuexis_core
    cuexis_json_support
    cuexis_project
    cuexis_platform_sdl
    cuexis_world
    cuexis_assets
    cuexis_chart
    cuexis_behavior
    cuexis_gameplay
    cuexis_render
    cuexis_debug
    cuexis_runtime
    cuexis_render_opengl
    cuexis_playback
    cuexis_player
)

if(BUILD_TESTING)
    list(APPEND CUEXIS_ACTIVE_TARGETS
        cuexis_core_tests
        cuexis_json_support_tests
        cuexis_project_tests
        cuexis_platform_sdl_tests
        cuexis_assets_tests
        cuexis_chart_tests
        cuexis_behavior_tests
        cuexis_gameplay_tests
        cuexis_debug_tests
        cuexis_render_tests
        cuexis_render_opengl_tests
        cuexis_runtime_tests
        cuexis_world_tests
        cuexis_playback_tests
    )

    add_test(
        NAME cuexis_architecture_tests
        COMMAND "${CMAKE_COMMAND}"
            "-DCUEXIS_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_SOURCE_DIR}/cmake/VerifyArchitecture.cmake"
    )
    add_test(
        NAME cuexis_player_rejects_unknown_argument
        COMMAND "${CMAKE_COMMAND}"
            "-DCUEXIS_PLAYER=$<TARGET_FILE:cuexis_player>"
            -DCUEXIS_FAILURE_CASE=unknown_argument
            -P "${CMAKE_SOURCE_DIR}/cmake/VerifyPlayerFailure.cmake"
    )
    add_test(
        NAME cuexis_player_reports_sdl_init_failure
        COMMAND "${CMAKE_COMMAND}"
            "-DCUEXIS_PLAYER=$<TARGET_FILE:cuexis_player>"
            -DCUEXIS_FAILURE_CASE=sdl_init
            -P "${CMAKE_SOURCE_DIR}/cmake/VerifyPlayerFailure.cmake"
    )
    add_test(
        NAME cuexis_player_requires_chart_path_value
        COMMAND "${CMAKE_COMMAND}"
            "-DCUEXIS_PLAYER=$<TARGET_FILE:cuexis_player>"
            -DCUEXIS_FAILURE_CASE=missing_chart_argument
            -P "${CMAKE_SOURCE_DIR}/cmake/VerifyPlayerFailure.cmake"
    )
    add_test(
        NAME cuexis_player_reports_missing_chart
        COMMAND "${CMAKE_COMMAND}"
            "-DCUEXIS_PLAYER=$<TARGET_FILE:cuexis_player>"
            -DCUEXIS_FAILURE_CASE=chart_open
            -P "${CMAKE_SOURCE_DIR}/cmake/VerifyPlayerFailure.cmake"
    )
    add_test(
        NAME cuexis_player_requires_project_path_value
        COMMAND "${CMAKE_COMMAND}"
            "-DCUEXIS_PLAYER=$<TARGET_FILE:cuexis_player>"
            -DCUEXIS_FAILURE_CASE=missing_project_argument
            -P "${CMAKE_SOURCE_DIR}/cmake/VerifyPlayerFailure.cmake"
    )
    add_test(
        NAME cuexis_player_rejects_project_chart_conflict
        COMMAND "${CMAKE_COMMAND}"
            "-DCUEXIS_PLAYER=$<TARGET_FILE:cuexis_player>"
            -DCUEXIS_FAILURE_CASE=project_chart_conflict
            -P "${CMAKE_SOURCE_DIR}/cmake/VerifyPlayerFailure.cmake"
    )
    add_test(
        NAME cuexis_player_reports_missing_project
        COMMAND "${CMAKE_COMMAND}"
            "-DCUEXIS_PLAYER=$<TARGET_FILE:cuexis_player>"
            -DCUEXIS_FAILURE_CASE=project_open
            -P "${CMAKE_SOURCE_DIR}/cmake/VerifyPlayerFailure.cmake"
    )
endif()

foreach(target IN LISTS CUEXIS_ACTIVE_TARGETS)
    cuexis_enable_warnings(${target})
endforeach()

file(GLOB_RECURSE CUEXIS_FORMAT_FILES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/app/*.cpp"
    "${CMAKE_SOURCE_DIR}/app/*.hpp"
    "${CMAKE_SOURCE_DIR}/cmake/*.hpp.in"
    "${CMAKE_SOURCE_DIR}/engine/*.cpp"
    "${CMAKE_SOURCE_DIR}/engine/*.hpp"
    "${CMAKE_SOURCE_DIR}/tests/*.cpp"
    "${CMAKE_SOURCE_DIR}/tests/*.hpp"
)

find_program(CUEXIS_CLANG_FORMAT_EXECUTABLE NAMES clang-format)
if(CUEXIS_CLANG_FORMAT_EXECUTABLE)
    add_custom_target(cuexis_format_check
        COMMAND "${CUEXIS_CLANG_FORMAT_EXECUTABLE}" --dry-run --Werror ${CUEXIS_FORMAT_FILES}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        VERBATIM
    )
else()
    add_custom_target(cuexis_format_check
        COMMAND "${CMAKE_COMMAND}" -E echo "clang-format was not found in PATH"
        COMMAND "${CMAKE_COMMAND}" -E false
        VERBATIM
    )
endif()

list(APPEND CUEXIS_ACTIVE_TARGETS cuexis_format_check)

cuexis_verify_target_dependencies(cuexis_core
    ALLOWED
        tl::expected
        fmt::fmt
        glm::glm
        spdlog::spdlog
)
cuexis_verify_target_dependencies(cuexis_platform_sdl
    ALLOWED
        cuexis::core
        SDL3::SDL3
)
cuexis_verify_target_dependencies(cuexis_json_support
    ALLOWED
        cuexis::core
        nlohmann_json::nlohmann_json
        nlohmann_json_schema_validator::validator
)
cuexis_verify_target_dependencies(cuexis_project
    ALLOWED
        cuexis::core
        cuexis::json_support
)
cuexis_verify_target_dependencies(cuexis_world
    ALLOWED
        cuexis::core
        EnTT::EnTT
)
cuexis_verify_target_dependencies(cuexis_assets
    ALLOWED
        cuexis::core
)
cuexis_verify_target_dependencies(cuexis_chart
    ALLOWED
        cuexis::core
        cuexis::json_support
)
cuexis_verify_target_dependencies(cuexis_behavior
    ALLOWED
        cuexis::core
        cuexis::world
)
cuexis_verify_target_dependencies(cuexis_gameplay
    ALLOWED
        cuexis::core
)
cuexis_verify_target_dependencies(cuexis_render
    ALLOWED
        cuexis::assets
        cuexis::core
)
cuexis_verify_target_dependencies(cuexis_debug
    ALLOWED
        cuexis::core
        cuexis::render
        cuexis::world
)
cuexis_verify_target_dependencies(cuexis_runtime
    ALLOWED
        cuexis::assets
        cuexis::behavior
        cuexis::chart
        cuexis::core
        cuexis::gameplay
        cuexis::render
        cuexis::world
)
cuexis_verify_target_dependencies(cuexis_render_opengl
    ALLOWED
        cuexis::render
        cuexis::platform_sdl
        glad::glad
        SDL3::SDL3
)
cuexis_verify_target_dependencies(cuexis_playback
    ALLOWED
        cuexis::core
        cuexis::assets
        cuexis::chart
        cuexis::render
        cuexis::runtime
        cuexis::world
)
cuexis_verify_target_dependencies(cuexis_player
    ALLOWED
        cuexis::playback
        cuexis::assets
        cuexis::core
        cuexis::platform_sdl
        cuexis::project
        cuexis::render
        cuexis::render_opengl
)

if(BUILD_TESTING)
    cuexis_verify_target_dependencies(cuexis_core_tests
        ALLOWED
            cuexis::core
            Catch2::Catch2WithMain
    )
    cuexis_verify_target_dependencies(cuexis_platform_sdl_tests
        ALLOWED
            cuexis::platform_sdl
            Catch2::Catch2WithMain
    )
    cuexis_verify_target_dependencies(cuexis_json_support_tests
        ALLOWED
            cuexis::json_support
            Catch2::Catch2WithMain
    )
    cuexis_verify_target_dependencies(cuexis_project_tests
        ALLOWED
            cuexis::project
            Catch2::Catch2WithMain
    )
    cuexis_verify_target_dependencies(cuexis_assets_tests
        ALLOWED
            cuexis::assets
            Catch2::Catch2WithMain
    )
    cuexis_verify_target_dependencies(cuexis_chart_tests
        ALLOWED
            cuexis::chart
            Catch2::Catch2WithMain
    )
    cuexis_verify_target_dependencies(cuexis_behavior_tests
        ALLOWED
            cuexis::behavior
            Catch2::Catch2WithMain
    )
    cuexis_verify_target_dependencies(cuexis_gameplay_tests
        ALLOWED
            cuexis::gameplay
            Catch2::Catch2WithMain
    )
    cuexis_verify_target_dependencies(cuexis_debug_tests
        ALLOWED
            cuexis::debug
            Catch2::Catch2WithMain
    )
    cuexis_verify_target_dependencies(cuexis_render_opengl_tests
        ALLOWED
            cuexis::render_opengl
            Catch2::Catch2WithMain
    )
    cuexis_verify_target_dependencies(cuexis_render_tests
        ALLOWED
            cuexis::render
            Catch2::Catch2WithMain
    )
    cuexis_verify_target_dependencies(cuexis_runtime_tests
        ALLOWED
            cuexis::assets
            cuexis::behavior
            cuexis::chart
            cuexis::gameplay
            cuexis::render
            cuexis::runtime
            cuexis::world
            Catch2::Catch2WithMain
    )
    cuexis_verify_target_dependencies(cuexis_world_tests
        ALLOWED
            cuexis::world
            Catch2::Catch2WithMain
    )
    cuexis_verify_target_dependencies(cuexis_playback_tests
        ALLOWED
            cuexis::playback
            Catch2::Catch2WithMain
    )
endif()

cuexis_verify_active_targets(
    SOURCE_DIR "${CMAKE_SOURCE_DIR}"
    ALLOWED ${CUEXIS_ACTIVE_TARGETS}
)
