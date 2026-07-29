cmake_minimum_required(VERSION 3.25)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CuexisVersion.cmake")

project(Cuexis VERSION ${CUEXIS_CMAKE_VERSION} LANGUAGES CXX)

include(CTest)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
include(GenerateExportHeader)

if(PROJECT_IS_TOP_LEVEL)
    set(CUEXIS_TOP_LEVEL_DEFAULT ON)
    set(CUEXIS_TESTS_DEFAULT "${BUILD_TESTING}")
else()
    set(CUEXIS_TOP_LEVEL_DEFAULT OFF)
    set(CUEXIS_TESTS_DEFAULT OFF)
endif()

option(CUEXIS_BUILD_PLAYER "Build the Cuexis Player application" ${CUEXIS_TOP_LEVEL_DEFAULT})
option(CUEXIS_BUILD_SDL_ADAPTER "Build the SDL platform adapter" ${CUEXIS_BUILD_PLAYER})
option(CUEXIS_BUILD_AUDIO_SDL_ADAPTER "Build the SDL audio adapter" ${CUEXIS_BUILD_PLAYER})
option(CUEXIS_BUILD_OPENGL_ADAPTER "Build the OpenGL render adapter" ${CUEXIS_BUILD_PLAYER})
option(CUEXIS_BUILD_DEVELOPER_TOOLS "Build repository developer-only targets"
       ${CUEXIS_TOP_LEVEL_DEFAULT})
option(CUEXIS_BUILD_TESTS "Build the Cuexis test suite" ${CUEXIS_TESTS_DEFAULT})
option(CUEXIS_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(CUEXIS_ENABLE_CLANG_TIDY "Run clang-tidy while compiling Cuexis targets" OFF)
option(CUEXIS_ENABLE_COVERAGE "Enable compiler coverage instrumentation" OFF)

if(CUEXIS_BUILD_TESTS AND NOT BUILD_TESTING)
    enable_testing()
endif()

if(CUEXIS_BUILD_OPENGL_ADAPTER AND NOT CUEXIS_BUILD_SDL_ADAPTER)
    message(FATAL_ERROR "CUEXIS_BUILD_OPENGL_ADAPTER requires CUEXIS_BUILD_SDL_ADAPTER")
endif()
if(CUEXIS_BUILD_PLAYER AND
   (NOT CUEXIS_BUILD_SDL_ADAPTER OR NOT CUEXIS_BUILD_AUDIO_SDL_ADAPTER OR
    NOT CUEXIS_BUILD_OPENGL_ADAPTER))
    message(FATAL_ERROR
        "CUEXIS_BUILD_PLAYER requires the SDL platform, SDL audio, and OpenGL adapters")
endif()

if(CUEXIS_ENABLE_SANITIZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "CUEXIS_ENABLE_SANITIZERS requires Clang or GCC")
    endif()
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
endif()

if(CUEXIS_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "CUEXIS_ENABLE_COVERAGE requires Clang or GCC")
    endif()
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

if(CUEXIS_ENABLE_CLANG_TIDY)
    find_program(CUEXIS_CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)
    set(CMAKE_CXX_CLANG_TIDY
        "${CUEXIS_CLANG_TIDY_EXECUTABLE};--config-file=${PROJECT_SOURCE_DIR}/.clang-tidy"
    )
endif()

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
endif()

configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/version.hpp.in"
    "${PROJECT_BINARY_DIR}/generated/cuexis/version.hpp"
    @ONLY
)

include("${PROJECT_SOURCE_DIR}/cmake/CuexisWarnings.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/VerifyArchitecture.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/CuexisLibraryType.cmake")

add_subdirectory(engine)
if(CUEXIS_BUILD_PLAYER)
    add_subdirectory(app)
endif()

if(CUEXIS_BUILD_TESTS)
    add_subdirectory(tests)
endif()

set(CUEXIS_ACTIVE_TARGETS
    cuexis_core
    cuexis_audio
    cuexis_filesystem
    cuexis_content
    cuexis_json_support
    cuexis_project
    cuexis_world
    cuexis_assets
    cuexis_chart
    cuexis_behavior
    cuexis_gameplay
    cuexis_render
    cuexis_debug
    cuexis_runtime
    cuexis_playback
)

if(CUEXIS_BUILD_SDL_ADAPTER)
    list(APPEND CUEXIS_ACTIVE_TARGETS cuexis_platform_sdl)
endif()
if(CUEXIS_BUILD_AUDIO_SDL_ADAPTER)
    list(APPEND CUEXIS_ACTIVE_TARGETS cuexis_audio_sdl)
endif()
if(CUEXIS_BUILD_OPENGL_ADAPTER)
    list(APPEND CUEXIS_ACTIVE_TARGETS cuexis_render_opengl)
endif()
if(CUEXIS_BUILD_PLAYER)
    list(APPEND CUEXIS_ACTIVE_TARGETS cuexis_player)
endif()

if(CUEXIS_BUILD_TESTS)
    list(APPEND CUEXIS_ACTIVE_TARGETS
        cuexis_core_tests
        cuexis_audio_tests
        cuexis_filesystem_tests
        cuexis_content_tests
        cuexis_json_support_tests
        cuexis_project_tests
        cuexis_assets_tests
        cuexis_chart_tests
        cuexis_behavior_tests
        cuexis_gameplay_tests
        cuexis_debug_tests
        cuexis_render_tests
        cuexis_runtime_tests
        cuexis_world_tests
        cuexis_playback_tests
    )

    if(TARGET cuexis_platform_sdl_tests)
        list(APPEND CUEXIS_ACTIVE_TARGETS cuexis_platform_sdl_tests)
    endif()
    if(TARGET cuexis_audio_sdl_tests)
        list(APPEND CUEXIS_ACTIVE_TARGETS cuexis_audio_sdl_tests)
    endif()
    if(TARGET cuexis_render_opengl_tests)
        list(APPEND CUEXIS_ACTIVE_TARGETS cuexis_render_opengl_tests)
    endif()
    if(TARGET cuexis_player_diagnostics_tests)
        list(APPEND CUEXIS_ACTIVE_TARGETS cuexis_player_diagnostics_tests)
    endif()

    add_test(
        NAME cuexis_architecture_tests
        COMMAND "${CMAKE_COMMAND}"
            "-DCUEXIS_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/cmake/VerifyArchitecture.cmake"
    )
    if(CUEXIS_BUILD_PLAYER)
        add_test(
            NAME cuexis_player_rejects_unknown_argument
            COMMAND "${CMAKE_COMMAND}"
                "-DCUEXIS_PLAYER=$<TARGET_FILE:cuexis_player>"
                -DCUEXIS_FAILURE_CASE=unknown_argument
                -P "${PROJECT_SOURCE_DIR}/cmake/VerifyPlayerFailure.cmake"
        )
        add_test(
            NAME cuexis_player_reports_sdl_init_failure
            COMMAND "${CMAKE_COMMAND}"
                "-DCUEXIS_PLAYER=$<TARGET_FILE:cuexis_player>"
                -DCUEXIS_FAILURE_CASE=sdl_init
                -P "${PROJECT_SOURCE_DIR}/cmake/VerifyPlayerFailure.cmake"
        )
        add_test(
            NAME cuexis_player_requires_chart_path_value
            COMMAND "${CMAKE_COMMAND}"
                "-DCUEXIS_PLAYER=$<TARGET_FILE:cuexis_player>"
                -DCUEXIS_FAILURE_CASE=missing_chart_argument
                -P "${PROJECT_SOURCE_DIR}/cmake/VerifyPlayerFailure.cmake"
        )
        add_test(
            NAME cuexis_player_reports_missing_chart
            COMMAND "${CMAKE_COMMAND}"
                "-DCUEXIS_PLAYER=$<TARGET_FILE:cuexis_player>"
                -DCUEXIS_FAILURE_CASE=chart_open
                -P "${PROJECT_SOURCE_DIR}/cmake/VerifyPlayerFailure.cmake"
        )
        add_test(
            NAME cuexis_player_requires_project_path_value
            COMMAND "${CMAKE_COMMAND}"
                "-DCUEXIS_PLAYER=$<TARGET_FILE:cuexis_player>"
                -DCUEXIS_FAILURE_CASE=missing_project_argument
                -P "${PROJECT_SOURCE_DIR}/cmake/VerifyPlayerFailure.cmake"
        )
        add_test(
            NAME cuexis_player_rejects_project_chart_conflict
            COMMAND "${CMAKE_COMMAND}"
                "-DCUEXIS_PLAYER=$<TARGET_FILE:cuexis_player>"
                -DCUEXIS_FAILURE_CASE=project_chart_conflict
                -P "${PROJECT_SOURCE_DIR}/cmake/VerifyPlayerFailure.cmake"
        )
        add_test(
            NAME cuexis_player_reports_missing_project
            COMMAND "${CMAKE_COMMAND}"
                "-DCUEXIS_PLAYER=$<TARGET_FILE:cuexis_player>"
                -DCUEXIS_FAILURE_CASE=project_open
                -P "${PROJECT_SOURCE_DIR}/cmake/VerifyPlayerFailure.cmake"
        )
    endif()

    if(PROJECT_IS_TOP_LEVEL)
        foreach(consumer_mode IN ITEMS
                add_subdirectory
                add_subdirectory_audio_sdl
                find_package
                find_package_audio_sdl)
            add_test(
                NAME cuexis_external_consumer_${consumer_mode}
                COMMAND "${CMAKE_COMMAND}"
                    "-DCUEXIS_CONSUMER_MODE=${consumer_mode}"
                    "-DCUEXIS_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
                    "-DCUEXIS_BINARY_DIR=${PROJECT_BINARY_DIR}"
                    "-DCUEXIS_GENERATOR=${CMAKE_GENERATOR}"
                    "-DCUEXIS_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
                    "-DCUEXIS_LIBRARY_TYPE=${CUEXIS_LIBRARY_TYPE}"
                    "-DCUEXIS_SDK_API_VERSION=${CUEXIS_SDK_API_VERSION}"
                    "-DCUEXIS_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
                    "-DCUEXIS_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
                    "-DCUEXIS_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
                    "-DCUEXIS_VCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}"
                    -P "${PROJECT_SOURCE_DIR}/cmake/VerifyExternalConsumer.cmake"
            )
            set_tests_properties(cuexis_external_consumer_${consumer_mode} PROPERTIES
                LABELS "external;package"
                RESOURCE_LOCK cuexis_external_consumer
                TIMEOUT 900
            )
        endforeach()
        if(CUEXIS_LIBRARY_TYPE STREQUAL "SHARED")
            if(MSVC)
                find_program(CUEXIS_SYMBOL_TOOL NAMES dumpbin REQUIRED)
                set(CUEXIS_SYMBOL_TOOL_KIND dumpbin)
            else()
                find_program(CUEXIS_SYMBOL_TOOL NAMES llvm-nm nm REQUIRED)
                set(CUEXIS_SYMBOL_TOOL_KIND nm)
            endif()
            add_test(
                NAME cuexis_shared_export_surface
                COMMAND "${CMAKE_COMMAND}"
                    "-DCUEXIS_SHARED_LIBRARY=$<TARGET_FILE:cuexis_playback>"
                    "-DCUEXIS_SYMBOL_TOOL=${CUEXIS_SYMBOL_TOOL}"
                    "-DCUEXIS_SYMBOL_TOOL_KIND=${CUEXIS_SYMBOL_TOOL_KIND}"
                    -P "${PROJECT_SOURCE_DIR}/cmake/VerifySharedExports.cmake"
            )
            set_tests_properties(cuexis_shared_export_surface PROPERTIES
                LABELS "shared;package"
            )
            if(MSVC)
                add_test(
                    NAME cuexis_shared_consumer_imports
                    COMMAND "${CMAKE_COMMAND}"
                        "-DCUEXIS_CONSUMER=${PROJECT_BINARY_DIR}/ec/p/build/cuexis_external_consumer.exe"
                        "-DCUEXIS_DUMPBIN=${CUEXIS_SYMBOL_TOOL}"
                        "-DCUEXIS_CORE_LIBRARY=$<TARGET_FILE_NAME:cuexis_core>"
                        "-DCUEXIS_CONTENT_LIBRARY=$<TARGET_FILE_NAME:cuexis_content>"
                        "-DCUEXIS_PLAYBACK_LIBRARY=$<TARGET_FILE_NAME:cuexis_playback>"
                        -P "${PROJECT_SOURCE_DIR}/cmake/VerifySharedConsumerImports.cmake"
                )
                set_tests_properties(cuexis_shared_consumer_imports PROPERTIES
                    DEPENDS cuexis_external_consumer_find_package
                    LABELS "shared;external;package"
                    RESOURCE_LOCK cuexis_external_consumer
                )
            endif()
        endif()
    endif()
endif()

foreach(target IN LISTS CUEXIS_ACTIVE_TARGETS)
    cuexis_enable_warnings(${target})
endforeach()

if(CUEXIS_BUILD_DEVELOPER_TOOLS)
    file(GLOB_RECURSE CUEXIS_FORMAT_FILES CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/app/*.cpp"
        "${PROJECT_SOURCE_DIR}/app/*.hpp"
        "${PROJECT_SOURCE_DIR}/cmake/*.hpp.in"
        "${PROJECT_SOURCE_DIR}/engine/*.cpp"
        "${PROJECT_SOURCE_DIR}/engine/*.hpp"
        "${PROJECT_SOURCE_DIR}/tests/*.cpp"
        "${PROJECT_SOURCE_DIR}/tests/*.hpp"
    )

    find_program(CUEXIS_CLANG_FORMAT_EXECUTABLE NAMES clang-format)
    if(CUEXIS_CLANG_FORMAT_EXECUTABLE)
        add_custom_target(cuexis_format_check
            COMMAND "${CUEXIS_CLANG_FORMAT_EXECUTABLE}" --dry-run --Werror
                    ${CUEXIS_FORMAT_FILES}
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
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
endif()

set(CUEXIS_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/Cuexis"
    CACHE STRING "Cuexis CMake package installation directory"
)
set(CUEXIS_PUBLIC_EXPORT_TARGETS
    cuexis_core
    cuexis_audio
    cuexis_content
    cuexis_playback
)
set(CUEXIS_STATIC_IMPLEMENTATION_TARGETS
    cuexis_filesystem
    cuexis_json_support
    cuexis_project
    cuexis_world
    cuexis_assets
    cuexis_chart
    cuexis_behavior
    cuexis_gameplay
    cuexis_render
    cuexis_runtime
)
set(CUEXIS_PLAYBACK_EXPORT_TARGETS ${CUEXIS_PUBLIC_EXPORT_TARGETS})
if(CUEXIS_LIBRARY_TYPE STREQUAL "STATIC")
    list(APPEND CUEXIS_PLAYBACK_EXPORT_TARGETS ${CUEXIS_STATIC_IMPLEMENTATION_TARGETS})
endif()

install(TARGETS ${CUEXIS_PUBLIC_EXPORT_TARGETS}
    EXPORT CuexisTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
if(CUEXIS_LIBRARY_TYPE STREQUAL "STATIC")
    install(TARGETS ${CUEXIS_STATIC_IMPLEMENTATION_TARGETS}
        EXPORT CuexisTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    )
endif()
if(TARGET cuexis_audio_sdl)
    install(TARGETS cuexis_audio_sdl
        EXPORT CuexisAudioSDLTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )
endif()
install(FILES "${PROJECT_BINARY_DIR}/generated/cuexis/version.hpp"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/cuexis"
)

configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/CuexisConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/CuexisConfig.cmake"
    INSTALL_DESTINATION "${CUEXIS_INSTALL_CMAKEDIR}"
)
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/CuexisConfigVersion.cmake"
    VERSION "${CUEXIS_SDK_API_VERSION}"
    COMPATIBILITY SameMinorVersion
)
install(EXPORT CuexisTargets
    FILE CuexisTargets.cmake
    NAMESPACE Cuexis::
    DESTINATION "${CUEXIS_INSTALL_CMAKEDIR}"
)
if(TARGET cuexis_audio_sdl)
    install(EXPORT CuexisAudioSDLTargets
        FILE CuexisAudioSDLTargets.cmake
        NAMESPACE Cuexis::
        DESTINATION "${CUEXIS_INSTALL_CMAKEDIR}"
    )
endif()
install(FILES
    "${PROJECT_BINARY_DIR}/CuexisConfig.cmake"
    "${PROJECT_BINARY_DIR}/CuexisConfigVersion.cmake"
    DESTINATION "${CUEXIS_INSTALL_CMAKEDIR}"
)
install(FILES
    "${PROJECT_SOURCE_DIR}/LICENSE"
    "${PROJECT_SOURCE_DIR}/NOTICE"
    "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/Cuexis"
)

set(CUEXIS_DISTRIBUTED_VCPKG_PORTS
    entt
    glm
    json-schema-validator
    nlohmann-json
    tl-expected
)
if(TARGET cuexis_audio_sdl)
    list(APPEND CUEXIS_DISTRIBUTED_VCPKG_PORTS sdl3)
endif()
foreach(port IN LISTS CUEXIS_DISTRIBUTED_VCPKG_PORTS)
    set(copyright_file
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/${port}/copyright")
    if(EXISTS "${copyright_file}")
        install(FILES "${copyright_file}"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/Cuexis/licenses"
            RENAME "${port}-copyright.txt"
        )
    endif()
endforeach()

cuexis_verify_target_dependencies(cuexis_core
    ALLOWED
        tl::expected
        glm::glm
)
cuexis_verify_target_dependencies(cuexis_audio
    ALLOWED
        cuexis::core
)
cuexis_verify_target_dependencies(cuexis_filesystem
    ALLOWED
        cuexis::core
)
cuexis_verify_target_dependencies(cuexis_content
    ALLOWED
        cuexis::core
        cuexis::filesystem
)
if(TARGET cuexis_platform_sdl)
    cuexis_verify_target_dependencies(cuexis_platform_sdl
        ALLOWED
            cuexis::core
            SDL3::SDL3
    )
endif()
if(TARGET cuexis_audio_sdl)
    cuexis_verify_target_dependencies(cuexis_audio_sdl
        ALLOWED
            cuexis::audio
            SDL3::SDL3
    )
endif()
cuexis_verify_target_dependencies(cuexis_json_support
    ALLOWED
        cuexis::core
        nlohmann_json::nlohmann_json
        nlohmann_json_schema_validator::validator
)
cuexis_verify_target_dependencies(cuexis_project
    ALLOWED
        cuexis::core
        cuexis::filesystem
        cuexis::json_support
)
cuexis_verify_target_dependencies(cuexis_world
    ALLOWED
        cuexis::core
        EnTT::EnTT
)
cuexis_verify_target_dependencies(cuexis_assets
    ALLOWED
        cuexis::content
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
if(TARGET cuexis_render_opengl)
    cuexis_verify_target_dependencies(cuexis_render_opengl
        ALLOWED
            cuexis::render
            cuexis::platform_sdl
            glad::glad
            SDL3::SDL3
    )
endif()
cuexis_verify_target_dependencies(cuexis_playback
    ALLOWED
        cuexis::audio
        cuexis::content
        cuexis::core
        cuexis::assets
        cuexis::chart
        cuexis::filesystem
        cuexis::project
        cuexis::render
        cuexis::runtime
        cuexis::world
)
if(TARGET cuexis_player)
    cuexis_verify_target_dependencies(cuexis_player
        ALLOWED
            cuexis::audio
            cuexis::audio_sdl
            cuexis::playback
            cuexis::core
            cuexis::filesystem
            cuexis::platform_sdl
            cuexis::render
            cuexis::render_opengl
            spdlog::spdlog
    )
endif()

if(CUEXIS_BUILD_TESTS)
    cuexis_verify_target_dependencies(cuexis_core_tests
        ALLOWED
            cuexis::core
            Catch2::Catch2WithMain
    )
    cuexis_verify_target_dependencies(cuexis_audio_tests
        ALLOWED
            cuexis::audio
            Catch2::Catch2WithMain
    )
    cuexis_verify_target_dependencies(cuexis_filesystem_tests
        ALLOWED
            cuexis::filesystem
            Catch2::Catch2WithMain
    )

    cuexis_verify_target_dependencies(cuexis_content_tests
        ALLOWED
            cuexis::content
            Catch2::Catch2WithMain
    )
    if(TARGET cuexis_platform_sdl_tests)
        cuexis_verify_target_dependencies(cuexis_platform_sdl_tests
            ALLOWED
                cuexis::platform_sdl
                Catch2::Catch2WithMain
        )
    endif()
    if(TARGET cuexis_audio_sdl_tests)
        cuexis_verify_target_dependencies(cuexis_audio_sdl_tests
            ALLOWED
                cuexis::audio
                cuexis::audio_sdl
                Catch2::Catch2WithMain
        )
    endif()
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
    if(TARGET cuexis_render_opengl_tests)
        cuexis_verify_target_dependencies(cuexis_render_opengl_tests
            ALLOWED
                cuexis::render_opengl
                Catch2::Catch2WithMain
        )
    endif()
    if(TARGET cuexis_player_diagnostics_tests)
        cuexis_verify_target_dependencies(cuexis_player_diagnostics_tests
            ALLOWED
                cuexis::audio
                cuexis::core
                cuexis::playback
                Catch2::Catch2WithMain
        )
    endif()
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
            cuexis::audio
            cuexis::assets
            cuexis::playback
            Catch2::Catch2WithMain
    )
endif()

cuexis_verify_active_targets(
    SOURCE_DIR "${PROJECT_SOURCE_DIR}"
    ALLOWED ${CUEXIS_ACTIVE_TARGETS}
)
