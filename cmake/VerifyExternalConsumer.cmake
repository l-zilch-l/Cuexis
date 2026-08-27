cmake_minimum_required(VERSION 3.25)

foreach(required_variable IN ITEMS
        CUEXIS_CONSUMER_MODE
        CUEXIS_SOURCE_DIR
        CUEXIS_BINARY_DIR
        CUEXIS_GENERATOR
        CUEXIS_BUILD_TYPE
        CUEXIS_LIBRARY_TYPE
        CUEXIS_SDK_API_VERSION)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

set(supported_consumer_modes
    add_subdirectory
    add_subdirectory_playback
    add_subdirectory_audio_sdl
    find_package
    find_package_playback
    find_package_core
    find_package_audio_sdl
)
if(NOT CUEXIS_CONSUMER_MODE IN_LIST supported_consumer_modes)
    message(FATAL_ERROR "Unsupported external consumer mode: ${CUEXIS_CONSUMER_MODE}")
endif()

function(cuexis_run_checked description)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE command_result
        OUTPUT_VARIABLE command_output
        ERROR_VARIABLE command_error
    )
    if(NOT command_result EQUAL 0)
        message(FATAL_ERROR
            "${description} failed with exit code ${command_result}\n"
            "stdout:\n${command_output}\n"
            "stderr:\n${command_error}"
        )
    endif()
endfunction()

function(cuexis_expect_configure_failure description)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE command_result
        OUTPUT_VARIABLE command_output
        ERROR_VARIABLE command_error
    )
    if(command_result EQUAL 0)
        message(FATAL_ERROR
            "${description} unexpectedly configured successfully\n"
            "stdout:\n${command_output}\n"
            "stderr:\n${command_error}"
        )
    endif()
endfunction()

if(CUEXIS_CONSUMER_MODE STREQUAL "add_subdirectory")
    set(consumer_work_id a)
elseif(CUEXIS_CONSUMER_MODE STREQUAL "add_subdirectory_playback")
    set(consumer_work_id ap)
elseif(CUEXIS_CONSUMER_MODE STREQUAL "add_subdirectory_audio_sdl")
    set(consumer_work_id as)
elseif(CUEXIS_CONSUMER_MODE STREQUAL "find_package")
    set(consumer_work_id p)
elseif(CUEXIS_CONSUMER_MODE STREQUAL "find_package_playback")
    set(consumer_work_id pp)
elseif(CUEXIS_CONSUMER_MODE STREQUAL "find_package_core")
    set(consumer_work_id pc)
else()
    set(consumer_work_id ps)
endif()

# Keep nested vcpkg try_compile paths below the Windows object-path limit.
set(work_dir "${CUEXIS_BINARY_DIR}/ec/${consumer_work_id}")
file(REMOVE_RECURSE "${work_dir}")
file(MAKE_DIRECTORY "${work_dir}")
set(fixture_dir "${work_dir}/fixture")
file(COPY "${CUEXIS_SOURCE_DIR}/assets/projects/stage3_project" DESTINATION "${fixture_dir}")
file(COPY
    "${CUEXIS_SOURCE_DIR}/tests/fixtures/chart_format_update/cfu_f_reference_project"
    "${CUEXIS_SOURCE_DIR}/tests/fixtures/chart_format_update/parameterized_project"
    "${CUEXIS_SOURCE_DIR}/tests/fixtures/chart_format_update/source_project"
    DESTINATION "${fixture_dir}")
file(COPY
    "${CUEXIS_SOURCE_DIR}/tests/fixtures/chart_format_update/golden/cfu_f_v4_reference.cxc"
    "${CUEXIS_SOURCE_DIR}/tests/fixtures/chart_format_update/golden/cxc_v1_v4_cxt.cxc"
    "${CUEXIS_SOURCE_DIR}/tests/fixtures/chart_format_update/valid/chart_v4_parameterized_rational.json"
    DESTINATION "${fixture_dir}")

set(playback_consumer_source "${CUEXIS_SOURCE_DIR}/tests/external/playback_consumer.cpp")
file(READ "${playback_consumer_source}" playback_consumer_contents)
string(REGEX MATCHALL
    "#[ \t]*include[ \t]*[<\"]cuexis/[^>\"]+[>\"]"
    playback_consumer_cuexis_includes
    "${playback_consumer_contents}")
if(NOT playback_consumer_cuexis_includes)
    message(FATAL_ERROR "Playback-only consumer does not include the installed Playback API")
endif()
foreach(include_line IN LISTS playback_consumer_cuexis_includes)
    if(NOT include_line MATCHES "cuexis/playback/")
        message(FATAL_ERROR
            "Playback-only consumer includes a non-Playback Cuexis header: ${include_line}")
    endif()
endforeach()
foreach(playback_consumer_cmake IN ITEMS
        "${CUEXIS_SOURCE_DIR}/tests/external/add_subdirectory_playback/CMakeLists.txt"
        "${CUEXIS_SOURCE_DIR}/tests/external/find_package_playback/CMakeLists.txt")
    file(READ "${playback_consumer_cmake}" playback_consumer_cmake_contents)
    if(playback_consumer_cmake_contents MATCHES
       "Cuexis::(Internal|Core|Content|Audio)" OR
       playback_consumer_cmake_contents MATCHES "cuexis_cxc")
        message(FATAL_ERROR
            "Playback-only consumer directly references a non-Playback Cuexis target")
    endif()
endforeach()

set(common_configure_arguments
    -G "${CUEXIS_GENERATOR}"
    "-DCMAKE_BUILD_TYPE=${CUEXIS_BUILD_TYPE}"
    "-DCUEXIS_LIBRARY_TYPE=${CUEXIS_LIBRARY_TYPE}"
)
if(DEFINED CUEXIS_CXX_COMPILER AND NOT CUEXIS_CXX_COMPILER STREQUAL "")
    list(APPEND common_configure_arguments
        "-DCMAKE_CXX_COMPILER=${CUEXIS_CXX_COMPILER}"
    )
endif()
if(DEFINED CUEXIS_MAKE_PROGRAM AND NOT CUEXIS_MAKE_PROGRAM STREQUAL "")
    list(APPEND common_configure_arguments
        "-DCMAKE_MAKE_PROGRAM=${CUEXIS_MAKE_PROGRAM}"
    )
endif()
if(DEFINED CUEXIS_RC_COMPILER AND NOT CUEXIS_RC_COMPILER STREQUAL "")
    list(APPEND common_configure_arguments
        "-DCMAKE_RC_COMPILER=${CUEXIS_RC_COMPILER}"
    )
endif()
if(DEFINED CUEXIS_MT AND NOT CUEXIS_MT STREQUAL "")
    list(APPEND common_configure_arguments
        "-DCMAKE_MT=${CUEXIS_MT}"
    )
endif()
if(DEFINED CUEXIS_TOOLCHAIN_FILE AND NOT CUEXIS_TOOLCHAIN_FILE STREQUAL "")
    list(APPEND common_configure_arguments
        "-DCMAKE_TOOLCHAIN_FILE=${CUEXIS_TOOLCHAIN_FILE}"
    )
endif()
if(DEFINED CUEXIS_VCPKG_TARGET_TRIPLET AND NOT CUEXIS_VCPKG_TARGET_TRIPLET STREQUAL "")
    list(APPEND common_configure_arguments
        "-DVCPKG_TARGET_TRIPLET=${CUEXIS_VCPKG_TARGET_TRIPLET}"
    )
endif()
if(DEFINED CUEXIS_VCPKG_MANIFEST_MODE AND NOT CUEXIS_VCPKG_MANIFEST_MODE STREQUAL "")
    list(APPEND common_configure_arguments
        "-DVCPKG_MANIFEST_MODE=${CUEXIS_VCPKG_MANIFEST_MODE}"
    )
endif()
if(DEFINED CUEXIS_VCPKG_INSTALLED_DIR AND NOT CUEXIS_VCPKG_INSTALLED_DIR STREQUAL "")
    list(APPEND common_configure_arguments
        "-DVCPKG_INSTALLED_DIR=${CUEXIS_VCPKG_INSTALLED_DIR}"
    )
endif()

if(CUEXIS_CONSUMER_MODE MATCHES "^add_subdirectory")
    set(consumer_build_dir "${work_dir}/build")
    set(consumer_source_dir
        "${CUEXIS_SOURCE_DIR}/tests/external/${CUEXIS_CONSUMER_MODE}")
    set(manifest_arguments -DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON)
    if(CUEXIS_CONSUMER_MODE STREQUAL "add_subdirectory_audio_sdl")
        list(APPEND manifest_arguments -DVCPKG_MANIFEST_FEATURES=audio-sdl)
    endif()
    cuexis_run_checked(
        "add_subdirectory consumer configure"
        "${CMAKE_COMMAND}"
        -S "${consumer_source_dir}"
        -B "${consumer_build_dir}"
        ${common_configure_arguments}
        "-DCUEXIS_SOURCE_DIR=${CUEXIS_SOURCE_DIR}"
        "-DCUEXIS_FIXTURE_DIR=${fixture_dir}"
        "-DVCPKG_MANIFEST_DIR=${CUEXIS_SOURCE_DIR}"
        ${manifest_arguments}
    )
else()
    set(package_build_dir "${work_dir}/package-build")
    set(package_prefix "${work_dir}/package")
    if(CUEXIS_CONSUMER_MODE STREQUAL "find_package_audio_sdl")
        set(package_audio_sdl ON)
        set(package_manifest_arguments
            -DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON
            -DVCPKG_MANIFEST_FEATURES=audio-sdl)
    else()
        set(package_audio_sdl OFF)
        set(package_manifest_arguments -DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON)
    endif()
    cuexis_run_checked(
        "Cuexis package configure"
        "${CMAKE_COMMAND}"
        -S "${CUEXIS_SOURCE_DIR}"
        -B "${package_build_dir}"
        ${common_configure_arguments}
        "-DCMAKE_INSTALL_PREFIX=${package_prefix}"
        -DBUILD_TESTING=OFF
        -DCUEXIS_BUILD_TESTS=OFF
        -DCUEXIS_BUILD_PLAYER=OFF
        -DCUEXIS_BUILD_SDL_ADAPTER=OFF
        "-DCUEXIS_BUILD_AUDIO_SDL_ADAPTER=${package_audio_sdl}"
        -DCUEXIS_BUILD_OPENGL_ADAPTER=OFF
        -DCUEXIS_BUILD_DEVELOPER_TOOLS=OFF
        ${package_manifest_arguments}
    )
    cuexis_run_checked(
        "Cuexis package build and install"
        "${CMAKE_COMMAND}" --build "${package_build_dir}" --target install
        --config "${CUEXIS_BUILD_TYPE}"
    )

    set(required_package_files
        include/cuexis/version.hpp
        include/cuexis/core/abi_warnings.hpp
        include/cuexis/core/core_export.hpp
        include/cuexis/content/content_provider.hpp
        include/cuexis/content/content_export.hpp
        include/cuexis/audio/audio_clip.hpp
        include/cuexis/audio/audio_config.hpp
        include/cuexis/audio/audio_transport.hpp
        include/cuexis/audio/audio_export.hpp
        include/cuexis/playback/content_provider.hpp
        include/cuexis/playback/frame_digest.hpp
        include/cuexis/playback/playback_export.hpp
        include/cuexis/playback/playback_session.hpp
        include/cuexis/playback/playback_source.hpp
        include/cuexis/playback/presentation.hpp
        lib/cmake/Cuexis/CuexisConfig.cmake
        lib/cmake/Cuexis/CuexisConfigVersion.cmake
        lib/cmake/Cuexis/CuexisTargets.cmake
        share/Cuexis/LICENSE
        share/Cuexis/NOTICE
        share/Cuexis/THIRD_PARTY_NOTICES.md
        share/Cuexis/licenses/entt-copyright.txt
        share/Cuexis/licenses/glm-copyright.txt
        share/Cuexis/licenses/json-schema-validator-copyright.txt
        share/Cuexis/licenses/nlohmann-json-copyright.txt
        share/Cuexis/licenses/tl-expected-copyright.txt
    )
    if(CUEXIS_LIBRARY_TYPE STREQUAL "STATIC")
        list(APPEND required_package_files
            share/Cuexis/licenses/minizip-ng-copyright.txt)
    endif()
    if(package_audio_sdl)
        list(APPEND required_package_files
            include/cuexis/audio_sdl/audio_sdl_export.hpp
            include/cuexis/audio_sdl/sdl_audio.hpp
            include/cuexis/audio_sdl/wav_decoder.hpp
            lib/cmake/Cuexis/CuexisAudioSDLTargets.cmake
            share/Cuexis/licenses/sdl3-copyright.txt)
    elseif(EXISTS "${package_prefix}/lib/cmake/Cuexis/CuexisAudioSDLTargets.cmake" OR
           EXISTS "${package_prefix}/include/cuexis/audio_sdl")
        message(FATAL_ERROR "Base Cuexis package contains AudioSDL artifacts")
    endif()
    foreach(relative_path IN LISTS required_package_files)
        if(NOT EXISTS "${package_prefix}/${relative_path}")
            message(FATAL_ERROR "Installed Cuexis package is missing ${relative_path}")
        endif()
    endforeach()

    foreach(internal_header_directory IN ITEMS
            assets behavior chart cxc filesystem gameplay json project render render_opengl runtime world)
        if(EXISTS "${package_prefix}/include/cuexis/${internal_header_directory}")
            message(FATAL_ERROR
                "Cuexis package installed internal ${internal_header_directory} headers")
        endif()
    endforeach()
    if(EXISTS "${package_prefix}/include/cuexis/platform")
        message(FATAL_ERROR "Cuexis package installed platform adapter headers")
    endif()
    if(NOT package_audio_sdl AND EXISTS "${package_prefix}/include/cuexis/audio_sdl")
        message(FATAL_ERROR "Base Cuexis package contains AudioSDL headers")
    endif()
    if(EXISTS "${package_prefix}/lib/cmake/Cuexis/CuexisRenderOpenGLTargets.cmake")
        message(FATAL_ERROR "Cuexis package contains an unsupported OpenGL component")
    endif()

    set(installed_targets_file "${package_prefix}/lib/cmake/Cuexis/CuexisTargets.cmake")
    file(READ "${installed_targets_file}" installed_targets)
    file(READ "${package_prefix}/lib/cmake/Cuexis/CuexisConfig.cmake" installed_config)
    foreach(forbidden_dependency IN ITEMS "Cuexis::RenderOpenGL" "Cuexis::PlatformSDL"
            "glad::" "OpenGL::" "SDL3::")
        if(installed_targets MATCHES "${forbidden_dependency}")
            message(FATAL_ERROR "Cuexis package leaks ${forbidden_dependency} in CuexisTargets")
        endif()
    endforeach()
    if(installed_config MATCHES "Cuexis::RenderOpenGL" OR installed_config MATCHES "glad::" OR
       installed_config MATCHES "OpenGL::")
        message(FATAL_ERROR "Cuexis package config leaks an unavailable adapter dependency")
    endif()
    if(CUEXIS_LIBRARY_TYPE STREQUAL "SHARED")
        foreach(private_target_or_dependency IN ITEMS
                "Cuexis::Internal" "EnTT::" "glm::" "nlohmann_json::"
                "MINIZIP::" "minizip-ng" "SDL3::" "glad::" "spdlog::")
            if(installed_targets MATCHES "${private_target_or_dependency}")
                message(FATAL_ERROR
                    "Shared Cuexis package leaks ${private_target_or_dependency}")
            endif()
        endforeach()
        if(installed_config MATCHES "MINIZIP::" OR installed_config MATCHES "minizip-ng")
            message(FATAL_ERROR "Shared Cuexis package config leaks minizip-ng")
        endif()
    else()
        foreach(required_internal_target IN ITEMS
                InternalAssets InternalChart InternalCxc InternalRuntime InternalWorld)
            if(NOT installed_targets MATCHES "Cuexis::${required_internal_target}")
                message(FATAL_ERROR
                    "Static Cuexis package is missing ${required_internal_target}")
            endif()
        endforeach()
        string(REGEX MATCH
            "set_target_properties\\(Cuexis::Playback PROPERTIES[^)]*\\)"
            installed_playback_target_block
            "${installed_targets}")
        if(NOT installed_playback_target_block MATCHES
           "LINK_ONLY:Cuexis::InternalCxc")
            message(FATAL_ERROR
                "Static Cuexis::Playback does not carry the InternalCxc link closure")
        endif()
    endif()

    if(NOT CUEXIS_SDK_API_VERSION MATCHES "^([0-9]+)[.]([0-9]+)[.][0-9]+$")
        message(FATAL_ERROR "Invalid CUEXIS_SDK_API_VERSION")
    endif()
    set(shared_name_version "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}")
    set(shared_debug_postfix "")
    if(CUEXIS_BUILD_TYPE STREQUAL "Debug")
        set(shared_debug_postfix "d")
    endif()
    if(CUEXIS_LIBRARY_TYPE STREQUAL "SHARED")
        set(required_shared_modules cuexis_core cuexis_content cuexis_audio cuexis_playback)
        if(package_audio_sdl)
            list(APPEND required_shared_modules cuexis_audio_sdl)
        endif()
        foreach(module IN LISTS required_shared_modules)
            set(shared_stem "${module}-${shared_name_version}${shared_debug_postfix}")
            if(CMAKE_HOST_WIN32)
                if(NOT EXISTS "${package_prefix}/bin/${shared_stem}.dll" OR
                   NOT EXISTS "${package_prefix}/lib/${shared_stem}.lib")
                    message(FATAL_ERROR
                        "Shared Cuexis package is missing ${shared_stem} runtime or import library")
                endif()
            else()
                file(GLOB shared_objects "${package_prefix}/lib/lib${shared_stem}.so*")
                if(NOT shared_objects)
                    message(FATAL_ERROR
                        "Shared Cuexis package is missing lib${shared_stem}.so")
                endif()
            endif()
        endforeach()
        if(CMAKE_HOST_WIN32)
            file(GLOB forbidden_internal_runtimes
                "${package_prefix}/bin/cuexis_assets*.dll"
                "${package_prefix}/bin/cuexis_runtime*.dll"
                "${package_prefix}/bin/cuexis_world*.dll")
        else()
            file(GLOB forbidden_internal_runtimes
                "${package_prefix}/lib/libcuexis_assets*.so*"
                "${package_prefix}/lib/libcuexis_runtime*.so*"
                "${package_prefix}/lib/libcuexis_world*.so*")
        endif()
        if(forbidden_internal_runtimes)
            message(FATAL_ERROR
                "Shared Cuexis package contains internal runtime libraries: "
                "${forbidden_internal_runtimes}")
        endif()
    endif()

    set(expected_license_files
        entt-copyright.txt
        glm-copyright.txt
        json-schema-validator-copyright.txt
        nlohmann-json-copyright.txt
        tl-expected-copyright.txt
    )
    if(CUEXIS_LIBRARY_TYPE STREQUAL "STATIC")
        list(APPEND expected_license_files minizip-ng-copyright.txt)
    endif()
    if(package_audio_sdl)
        list(APPEND expected_license_files sdl3-copyright.txt)
    endif()
    file(GLOB installed_license_files RELATIVE
        "${package_prefix}/share/Cuexis/licenses"
        "${package_prefix}/share/Cuexis/licenses/*")
    foreach(installed_license IN LISTS installed_license_files)
        if(NOT installed_license IN_LIST expected_license_files)
            message(FATAL_ERROR "Cuexis package installed an unexpected license file ${installed_license}")
        endif()
    endforeach()
    foreach(expected_license IN LISTS expected_license_files)
        if(NOT expected_license IN_LIST installed_license_files)
            message(FATAL_ERROR "Cuexis package is missing license file ${expected_license}")
        endif()
    endforeach()

    file(GLOB installed_playback_headers
        "${package_prefix}/include/cuexis/playback/*.hpp")
    foreach(header IN LISTS installed_playback_headers)
        file(READ "${header}" header_contents)
        if(header_contents MATCHES
           "#[ \t]*include[ \t]*[<\"](entt/|SDL|glad/|GL/|nlohmann/|spdlog/)")
            message(FATAL_ERROR
                "Installed Playback header leaked an implementation dependency: ${header}")
        endif()
        if(header_contents MATCHES "cuexis/cxc/|Cuexis::InternalCxc|cuexis_cxc")
            message(FATAL_ERROR
                "Installed Playback header leaked the internal CXC implementation: ${header}")
        endif()
    endforeach()

    file(GLOB_RECURSE installed_public_headers "${package_prefix}/include/cuexis/*.hpp")
    foreach(header IN LISTS installed_public_headers)
        file(READ "${header}" header_hex HEX)
        string(LENGTH "${header_hex}" header_hex_length)
        set(header_hex_offset 0)
        while(header_hex_offset LESS header_hex_length)
            string(SUBSTRING "${header_hex}" ${header_hex_offset} 2 header_byte_hex)
            math(EXPR header_byte "0x${header_byte_hex}")
            if(header_byte GREATER 127)
                message(FATAL_ERROR "Installed public header is not pure ASCII: ${header}")
            endif()
            math(EXPR header_hex_offset "${header_hex_offset} + 2")
        endwhile()
    endforeach()

    set(consumer_build_dir "${work_dir}/build")
    if(DEFINED CUEXIS_VCPKG_INSTALLED_DIR AND NOT CUEXIS_VCPKG_INSTALLED_DIR STREQUAL "")
        set(dependency_root "${CUEXIS_VCPKG_INSTALLED_DIR}")
        set(dependency_prefix
            "${dependency_root}/${CUEXIS_VCPKG_TARGET_TRIPLET}")
    else()
        set(dependency_root "${package_build_dir}/vcpkg_installed")
        set(dependency_prefix
            "${dependency_root}/${CUEXIS_VCPKG_TARGET_TRIPLET}")
    endif()
    set(consumer_configure_arguments ${common_configure_arguments})
    list(FILTER consumer_configure_arguments EXCLUDE
        REGEX "^-DVCPKG_(MANIFEST_MODE|INSTALLED_DIR)=")
    list(APPEND consumer_configure_arguments
        -DVCPKG_MANIFEST_MODE=OFF
        "-DVCPKG_INSTALLED_DIR=${dependency_root}"
        "-DCUEXIS_SOURCE_DIR=${CUEXIS_SOURCE_DIR}"
        "-DCUEXIS_FIXTURE_DIR=${fixture_dir}"
    )
    if(CUEXIS_CONSUMER_MODE STREQUAL "find_package")
        set(consumer_source_dir "${CUEXIS_SOURCE_DIR}/tests/external/find_package")
        set(component_arguments -DCMAKE_DISABLE_FIND_PACKAGE_SDL3=TRUE)
    elseif(CUEXIS_CONSUMER_MODE STREQUAL "find_package_playback")
        set(consumer_source_dir "${CUEXIS_SOURCE_DIR}/tests/external/find_package_playback")
        set(component_arguments -DCMAKE_DISABLE_FIND_PACKAGE_SDL3=TRUE)
    elseif(CUEXIS_CONSUMER_MODE STREQUAL "find_package_core")
        set(consumer_source_dir "${CUEXIS_SOURCE_DIR}/tests/external/find_package_core")
        set(component_arguments -DCMAKE_DISABLE_FIND_PACKAGE_SDL3=TRUE)
    else()
        set(consumer_source_dir
            "${CUEXIS_SOURCE_DIR}/tests/external/find_package_audio_sdl")
        set(component_arguments)
    endif()
    if(CUEXIS_CONSUMER_MODE STREQUAL "find_package_playback")
        foreach(invalid_version IN ITEMS 0.5 0.7)
            cuexis_expect_configure_failure(
                "Cuexis package version ${invalid_version} compatibility rejection"
                "${CMAKE_COMMAND}"
                -S "${consumer_source_dir}"
                -B "${work_dir}/version-${invalid_version}"
                ${consumer_configure_arguments}
                "-DCUEXIS_FIND_VERSION=${invalid_version}"
                "-DCuexis_DIR=${package_prefix}/lib/cmake/Cuexis"
                "-DCMAKE_PREFIX_PATH=${dependency_prefix}"
                "-DCUEXIS_EXPECTED_LIBRARY_TYPE=${CUEXIS_LIBRARY_TYPE}"
                ${component_arguments}
            )
        endforeach()
        cuexis_expect_configure_failure(
            "Cuexis package OpenGL component rejection"
            "${CMAKE_COMMAND}"
            -S "${consumer_source_dir}"
            -B "${work_dir}/unsupported-opengl-component"
            ${consumer_configure_arguments}
            -DCUEXIS_COMPONENTS=OpenGL
            "-DCuexis_DIR=${package_prefix}/lib/cmake/Cuexis"
            "-DCMAKE_PREFIX_PATH=${dependency_prefix}"
            "-DCUEXIS_EXPECTED_LIBRARY_TYPE=${CUEXIS_LIBRARY_TYPE}"
            ${component_arguments}
        )
    endif()
    if(CUEXIS_LIBRARY_TYPE STREQUAL "SHARED")
        if(CUEXIS_BUILD_TYPE STREQUAL "Debug")
            set(mismatched_build_type Release)
        else()
            set(mismatched_build_type Debug)
        endif()
        cuexis_expect_configure_failure(
            "shared package configuration mismatch"
            "${CMAKE_COMMAND}"
            -S "${consumer_source_dir}"
            -B "${work_dir}/configuration-mismatch"
            ${consumer_configure_arguments}
            "-DCMAKE_BUILD_TYPE=${mismatched_build_type}"
            "-DCuexis_DIR=${package_prefix}/lib/cmake/Cuexis"
            "-DCMAKE_PREFIX_PATH=${dependency_prefix}"
            "-DCUEXIS_EXPECTED_LIBRARY_TYPE=${CUEXIS_LIBRARY_TYPE}"
            ${component_arguments}
        )
        if(CMAKE_HOST_WIN32 AND CUEXIS_CXX_COMPILER MATCHES "cl([.]exe)?$")
            if(CUEXIS_BUILD_TYPE STREQUAL "Debug")
                set(static_runtime MultiThreadedDebug)
            else()
                set(static_runtime MultiThreaded)
            endif()
            cuexis_expect_configure_failure(
                "shared package static MSVC runtime mismatch"
                "${CMAKE_COMMAND}"
                -S "${consumer_source_dir}"
                -B "${work_dir}/runtime-mismatch"
                ${consumer_configure_arguments}
                "-DCMAKE_MSVC_RUNTIME_LIBRARY=${static_runtime}"
                "-DCuexis_DIR=${package_prefix}/lib/cmake/Cuexis"
                "-DCMAKE_PREFIX_PATH=${dependency_prefix}"
                "-DCUEXIS_EXPECTED_LIBRARY_TYPE=${CUEXIS_LIBRARY_TYPE}"
                ${component_arguments}
            )
        endif()
    endif()
    cuexis_run_checked(
        "find_package consumer configure"
        "${CMAKE_COMMAND}"
        -S "${consumer_source_dir}"
        -B "${consumer_build_dir}"
        ${consumer_configure_arguments}
        "-DCuexis_DIR=${package_prefix}/lib/cmake/Cuexis"
        "-DCMAKE_PREFIX_PATH=${dependency_prefix}"
        "-DCUEXIS_EXPECTED_LIBRARY_TYPE=${CUEXIS_LIBRARY_TYPE}"
        ${component_arguments}
    )
endif()

cuexis_run_checked(
    "external consumer build"
    "${CMAKE_COMMAND}" --build "${consumer_build_dir}" --target cuexis_external_consumer
    --config "${CUEXIS_BUILD_TYPE}"
)
if(CUEXIS_LIBRARY_TYPE STREQUAL "SHARED")
    if(CUEXIS_CONSUMER_MODE MATCHES "^find_package")
        if(CMAKE_HOST_WIN32)
            file(GLOB package_runtime_files "${package_prefix}/bin/*.dll")
        else()
            file(GLOB package_runtime_files "${package_prefix}/lib/*.so*")
        endif()
        if(NOT package_runtime_files)
            message(FATAL_ERROR "Shared Cuexis package contains no runtime libraries")
        endif()
        file(COPY ${package_runtime_files} DESTINATION "${consumer_build_dir}")
    else()
        if(CMAKE_HOST_WIN32)
            file(GLOB_RECURSE add_subdirectory_runtime_files
                "${consumer_build_dir}/cuexis/*.dll")
        else()
            file(GLOB_RECURSE add_subdirectory_runtime_files
                "${consumer_build_dir}/cuexis/*.so*")
        endif()
        if(NOT add_subdirectory_runtime_files)
            message(FATAL_ERROR "Shared add_subdirectory build contains no runtime libraries")
        endif()
        file(COPY ${add_subdirectory_runtime_files} DESTINATION "${consumer_build_dir}")
    endif()
endif()
if(CUEXIS_LIBRARY_TYPE STREQUAL "SHARED" AND NOT CMAKE_HOST_WIN32)
    cuexis_run_checked(
        "external consumer run"
        "${CMAKE_COMMAND}" -E env "LD_LIBRARY_PATH=${consumer_build_dir}"
        "${CMAKE_CTEST_COMMAND}" --test-dir "${consumer_build_dir}" --output-on-failure
        -C "${CUEXIS_BUILD_TYPE}"
    )
else()
    cuexis_run_checked(
        "external consumer run"
        "${CMAKE_CTEST_COMMAND}" --test-dir "${consumer_build_dir}" --output-on-failure
        -C "${CUEXIS_BUILD_TYPE}"
    )
endif()
