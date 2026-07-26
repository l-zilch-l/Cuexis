cmake_minimum_required(VERSION 3.25)

foreach(required_variable IN ITEMS
        CUEXIS_CONSUMER_MODE
        CUEXIS_SOURCE_DIR
        CUEXIS_BINARY_DIR
        CUEXIS_GENERATOR
        CUEXIS_BUILD_TYPE)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

if(NOT CUEXIS_CONSUMER_MODE STREQUAL "add_subdirectory" AND
   NOT CUEXIS_CONSUMER_MODE STREQUAL "find_package")
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

set(work_dir "${CUEXIS_BINARY_DIR}/external-consumer/${CUEXIS_CONSUMER_MODE}")
file(REMOVE_RECURSE "${work_dir}")
file(MAKE_DIRECTORY "${work_dir}")

set(common_configure_arguments
    -G "${CUEXIS_GENERATOR}"
    "-DCMAKE_BUILD_TYPE=${CUEXIS_BUILD_TYPE}"
)
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

if(CUEXIS_CONSUMER_MODE STREQUAL "add_subdirectory")
    set(consumer_build_dir "${work_dir}/build")
    cuexis_run_checked(
        "add_subdirectory consumer configure"
        "${CMAKE_COMMAND}"
        -S "${CUEXIS_SOURCE_DIR}/tests/external/add_subdirectory"
        -B "${consumer_build_dir}"
        ${common_configure_arguments}
        "-DCUEXIS_SOURCE_DIR=${CUEXIS_SOURCE_DIR}"
        "-DVCPKG_MANIFEST_DIR=${CUEXIS_SOURCE_DIR}"
        -DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON
    )
else()
    set(package_build_dir "${work_dir}/package-build")
    set(package_prefix "${work_dir}/package")
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
        -DCUEXIS_BUILD_OPENGL_ADAPTER=OFF
        -DCUEXIS_BUILD_DEVELOPER_TOOLS=OFF
        -DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON
    )
    cuexis_run_checked(
        "Cuexis package build and install"
        "${CMAKE_COMMAND}" --build "${package_build_dir}" --target install
        --config "${CUEXIS_BUILD_TYPE}"
    )

    set(required_package_files
        include/cuexis/version.hpp
        include/cuexis/content/content_provider.hpp
        include/cuexis/playback/content_provider.hpp
        include/cuexis/playback/playback_session.hpp
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
    foreach(relative_path IN LISTS required_package_files)
        if(NOT EXISTS "${package_prefix}/${relative_path}")
            message(FATAL_ERROR "Installed Cuexis package is missing ${relative_path}")
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
    endforeach()

    set(consumer_build_dir "${work_dir}/build")
    set(dependency_prefix
        "${package_build_dir}/vcpkg_installed/${CUEXIS_VCPKG_TARGET_TRIPLET}")
    cuexis_run_checked(
        "find_package consumer configure"
        "${CMAKE_COMMAND}"
        -S "${CUEXIS_SOURCE_DIR}/tests/external/find_package"
        -B "${consumer_build_dir}"
        ${common_configure_arguments}
        "-DCuexis_DIR=${package_prefix}/lib/cmake/Cuexis"
        "-DCMAKE_PREFIX_PATH=${dependency_prefix}"
    )
endif()

cuexis_run_checked(
    "external consumer build"
    "${CMAKE_COMMAND}" --build "${consumer_build_dir}" --target cuexis_external_consumer
    --config "${CUEXIS_BUILD_TYPE}"
)
cuexis_run_checked(
    "external consumer run"
    "${CMAKE_CTEST_COMMAND}" --test-dir "${consumer_build_dir}" --output-on-failure
    -C "${CUEXIS_BUILD_TYPE}"
)
