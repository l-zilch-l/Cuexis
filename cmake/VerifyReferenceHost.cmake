# Verifies the Cuexis Reference Host (ADR 0042 S6-D08, stage plan S6-C4).
#
# The gate proves, from a clean staging directory and with the source tree
# outside the project:
#   * the SDK installs into a prefix that is not the build tree,
#   * an out-of-tree host project configures against that prefix through
#     find_package(Cuexis 0.7.0 CONFIG REQUIRED COMPONENTS Playback),
#   * the host uses only installed public Playback headers and the exported
#     package target (no internal target, no Player configuration library, no
#     source-tree include),
#   * the host builds, starts, loads, consumes frames, seeks, reloads, rejects a
#     host content failure without disturbing active content, consumes a
#     published CXC package and destroys the session,
#   * the recorded identity and frame digest match the CFU-F reference goldens,
#   * the run does not depend on the development machine PATH,
#   * a package whose recorded toolchain disagrees with the consumer is refused.
#
# Required variables: CUEXIS_SOURCE_DIR, CUEXIS_BINARY_DIR, CUEXIS_GENERATOR,
# CUEXIS_BUILD_TYPE, CUEXIS_LIBRARY_TYPE, CUEXIS_CXX_COMPILER,
# CUEXIS_MAKE_PROGRAM, CUEXIS_RC_COMPILER, CUEXIS_MT, CUEXIS_TOOLCHAIN_FILE,
# CUEXIS_VCPKG_TARGET_TRIPLET, CUEXIS_VCPKG_MANIFEST_MODE,
# CUEXIS_VCPKG_INSTALLED_DIR.

foreach(required IN ITEMS
        CUEXIS_SOURCE_DIR
        CUEXIS_BINARY_DIR
        CUEXIS_GENERATOR
        CUEXIS_BUILD_TYPE
        CUEXIS_LIBRARY_TYPE
        CUEXIS_CXX_COMPILER
        CUEXIS_MAKE_PROGRAM)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

set(work_dir "${CUEXIS_BINARY_DIR}/ec/host")
file(REMOVE_RECURSE "${work_dir}")
file(MAKE_DIRECTORY "${work_dir}")

function(cuexis_host_run_checked label)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "${label} failed with ${result}\nstdout:\n${output}\nstderr:\n${error}")
    endif()
endfunction()

function(cuexis_host_expect_failure label expected_text)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error)
    if(result EQUAL 0)
        message(FATAL_ERROR "${label} was accepted but had to be refused")
    endif()
    set(combined "${output}\n${error}")
    if(NOT combined MATCHES "${expected_text}")
        message(FATAL_ERROR
            "${label} failed without the documented reason '${expected_text}'\n"
            "stdout:\n${output}\nstderr:\n${error}")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# 1. Source hygiene: the example must not reach into the Cuexis source tree.
# ---------------------------------------------------------------------------
set(host_source_dir "${CUEXIS_SOURCE_DIR}/examples/reference_host")
file(GLOB host_sources "${host_source_dir}/src/*.cpp" "${host_source_dir}/src/*.hpp")
if(NOT host_sources)
    message(FATAL_ERROR "The reference host has no sources")
endif()
foreach(source IN LISTS host_sources)
    file(READ "${source}" contents)
    if(contents MATCHES "#[ \t]*include[ \t]*\"\\.\\.")
        message(FATAL_ERROR "The reference host uses a relative source-tree include: ${source}")
    endif()
    string(REGEX MATCHALL "#[ \t]*include[ \t]*[<\"]cuexis/[^>\"]+[>\"]" includes "${contents}")
    foreach(include_line IN LISTS includes)
        if(NOT include_line MATCHES "cuexis/playback/")
            message(FATAL_ERROR
                "The reference host includes a non-Playback Cuexis header: ${include_line}")
        endif()
    endforeach()
endforeach()
file(READ "${host_source_dir}/CMakeLists.txt" host_cmake)
if(host_cmake MATCHES "Cuexis::(Internal|Core|Content|Audio)" OR
   host_cmake MATCHES "cuexis_cxc" OR
   host_cmake MATCHES "player_support")
    message(FATAL_ERROR "The reference host links an internal or Player-only Cuexis target")
endif()
if(host_cmake MATCHES "add_subdirectory[ \t]*\\([ \t]*\\$\\{?CUEXIS_SOURCE_DIR" OR
   host_cmake MATCHES "CUEXIS_SOURCE_DIR")
    message(FATAL_ERROR "The reference host depends on the Cuexis source tree")
endif()
if(NOT host_cmake MATCHES "find_package\\(Cuexis")
    message(FATAL_ERROR "The reference host must consume the installed package with find_package")
endif()

# ---------------------------------------------------------------------------
# 2. Clean staging: install the SDK outside the build tree.
# ---------------------------------------------------------------------------
set(prefix "${work_dir}/prefix")
cuexis_host_run_checked(
    "Cuexis install into the staging prefix"
    "${CMAKE_COMMAND}" --install "${CUEXIS_BINARY_DIR}"
    --prefix "${prefix}"
    --config "${CUEXIS_BUILD_TYPE}")

foreach(installed IN ITEMS
        include/cuexis/playback/playback_session.hpp
        include/cuexis/playback/playback_source.hpp
        include/cuexis/playback/content_provider.hpp
        include/cuexis/playback/frame_digest.hpp
        lib/cmake/Cuexis/CuexisConfig.cmake
        lib/cmake/Cuexis/CuexisConfigVersion.cmake
        lib/cmake/Cuexis/CuexisTargets.cmake)
    if(NOT EXISTS "${prefix}/${installed}")
        message(FATAL_ERROR "The staging prefix is missing ${installed}")
    endif()
endforeach()

# The staging prefix must not be the build tree or the source tree.
file(RELATIVE_PATH prefix_to_binary "${prefix}" "${CUEXIS_BINARY_DIR}")
if(NOT prefix_to_binary MATCHES "^\\.\\.")
    message(FATAL_ERROR "The staging prefix overlaps the build tree")
endif()

# ---------------------------------------------------------------------------
# 3. Out-of-tree host project, configured against the installed package only.
# ---------------------------------------------------------------------------
set(host_project "${work_dir}/host")
file(COPY "${host_source_dir}/" DESTINATION "${host_project}")
set(content_dir "${work_dir}/content")
file(COPY "${CUEXIS_SOURCE_DIR}/tests/fixtures/chart_format_update/cfu_f_reference_project"
    DESTINATION "${content_dir}")
file(COPY "${CUEXIS_SOURCE_DIR}/tests/fixtures/chart_format_update/golden/cfu_f_v4_reference.cxc"
    DESTINATION "${content_dir}")

set(host_build "${work_dir}/host-build")
# Arguments the host project needs to resolve its own dependency set. The
# foreign-toolchain case below reuses them so that it fails for exactly one
# documented reason.
set(vcpkg_arguments "")

# An instrumented SDK build must give its consumer the same instrumentation:
# Cuexis applies sanitizer and coverage flags as directory options, so an
# external project that links the installed static libraries has to mirror them
# or the link fails on undefined __asan_/__ubsan_/__gcov_ symbols.
set(instrumentation_arguments "")
if(CUEXIS_ENABLE_SANITIZERS)
    set(instrumentation_arguments
        "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer"
        "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer")
elseif(CUEXIS_ENABLE_COVERAGE)
    # --coverage on the linker line may expand to -lgcov before the static
    # archives; appending it to the standard libraries makes the resolution
    # independent of link order.
    set(instrumentation_arguments
        "-DCMAKE_CXX_FLAGS=--coverage -O0 -g"
        "-DCMAKE_EXE_LINKER_FLAGS=--coverage"
        "-DCMAKE_CXX_STANDARD_LIBRARIES=--coverage")
endif()

set(configure_arguments
    -S "${host_project}"
    -B "${host_build}"
    -G "${CUEXIS_GENERATOR}"
    "-DCMAKE_BUILD_TYPE=${CUEXIS_BUILD_TYPE}"
    "-DCMAKE_CXX_COMPILER=${CUEXIS_CXX_COMPILER}"
    "-DCMAKE_MAKE_PROGRAM=${CUEXIS_MAKE_PROGRAM}"
    "-DCuexis_DIR=${prefix}/lib/cmake/Cuexis"
    "-DCMAKE_PREFIX_PATH=${prefix}"
    "-DCUEXIS_HOST_API_VERSION=0.7.0"
    "-DCUEXIS_HOST_CONTENT_DIR=${content_dir}/cfu_f_reference_project"
    ${instrumentation_arguments})
if(DEFINED CUEXIS_RC_COMPILER AND NOT "${CUEXIS_RC_COMPILER}" STREQUAL "")
    list(APPEND configure_arguments "-DCMAKE_RC_COMPILER=${CUEXIS_RC_COMPILER}")
endif()
if(DEFINED CUEXIS_MT AND NOT "${CUEXIS_MT}" STREQUAL "")
    list(APPEND configure_arguments "-DCMAKE_MT=${CUEXIS_MT}")
endif()
if(DEFINED CUEXIS_TOOLCHAIN_FILE AND NOT "${CUEXIS_TOOLCHAIN_FILE}" STREQUAL "")
    # The host project resolves its own dependency set. It must never reuse the
    # Cuexis build tree's vcpkg_installed directory: a manifest install there
    # would prune packages the Cuexis build depends on.
    list(APPEND configure_arguments "-DCMAKE_TOOLCHAIN_FILE=${CUEXIS_TOOLCHAIN_FILE}")
    list(APPEND configure_arguments "-DVCPKG_MANIFEST_DIR=${CUEXIS_SOURCE_DIR}")
    list(APPEND configure_arguments "-DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON")
    list(APPEND configure_arguments "-DVCPKG_INSTALLED_DIR=${work_dir}/vcpkg-installed")
    list(APPEND vcpkg_arguments
        "-DCMAKE_TOOLCHAIN_FILE=${CUEXIS_TOOLCHAIN_FILE}"
        "-DVCPKG_MANIFEST_DIR=${CUEXIS_SOURCE_DIR}"
        "-DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON"
        "-DVCPKG_INSTALLED_DIR=${work_dir}/vcpkg-installed")
endif()
if(DEFINED CUEXIS_VCPKG_TARGET_TRIPLET AND NOT "${CUEXIS_VCPKG_TARGET_TRIPLET}" STREQUAL "")
    list(APPEND configure_arguments "-DVCPKG_TARGET_TRIPLET=${CUEXIS_VCPKG_TARGET_TRIPLET}")
    list(APPEND vcpkg_arguments "-DVCPKG_TARGET_TRIPLET=${CUEXIS_VCPKG_TARGET_TRIPLET}")
endif()

cuexis_host_run_checked("Reference host configure" "${CMAKE_COMMAND}" ${configure_arguments})
cuexis_host_run_checked(
    "Reference host build"
    "${CMAKE_COMMAND}" --build "${host_build}" --target cuexis_reference_host)

# ---------------------------------------------------------------------------
# 4. Run from a clean environment: the host must not need the development PATH.
# ---------------------------------------------------------------------------
set(host_executable "")
foreach(candidate IN ITEMS
        "${host_build}/cuexis_reference_host.exe"
        "${host_build}/cuexis_reference_host")
    if(EXISTS "${candidate}")
        set(host_executable "${candidate}")
        break()
    endif()
endforeach()
if(host_executable STREQUAL "")
    file(GLOB host_candidates "${host_build}/cuexis_reference_host*")
    message(FATAL_ERROR
        "The reference host executable was not produced; build directory contains: "
        "${host_candidates}")
endif()

if(CUEXIS_LIBRARY_TYPE STREQUAL "SHARED")
    file(GLOB runtime_libraries "${prefix}/bin/*.dll" "${prefix}/lib/*.so*")
    foreach(library IN LISTS runtime_libraries)
        file(COPY "${library}" DESTINATION "${host_build}")
    endforeach()
endif()

# A compiler runtime is not a Cuexis package file, but the host executable needs
# it in the sanitized environment: a MinGW build requires libgcc, libstdc++ and
# libwinpthread next to the executable, exactly as the Player distribution does.
get_filename_component(compiler_dir "${CUEXIS_CXX_COMPILER}" DIRECTORY)
file(GLOB toolchain_runtime_libraries
    "${compiler_dir}/libgcc_s_*.dll"
    "${compiler_dir}/libstdc++-6.dll"
    "${compiler_dir}/libwinpthread-1.dll")
foreach(library IN LISTS toolchain_runtime_libraries)
    file(COPY "${library}" DESTINATION "${host_build}")
endforeach()

set(report_path "${work_dir}/host-report.txt")
set(golden_identity "6d01494c126f3ae8fc9420259dc92873233022dec9dd6bf9caf04b217f100cc5")
set(golden_digest "1=11596562486377158370")
set(host_arguments
    --content "${content_dir}/cfu_f_reference_project"
    --package "${content_dir}/cfu_f_v4_reference.cxc"
    --expect-identity "${golden_identity}"
    --expect-digest "${golden_digest}"
    --report "${report_path}")

if(WIN32)
    set(clean_path "${host_build};$ENV{SystemRoot}/System32;$ENV{SystemRoot}")
else()
    set(clean_path "/usr/bin:/bin")
endif()

# The run must not depend on the development machine PATH: only the directory
# that holds the host runtime libraries plus the system directories remain.
set(saved_path "$ENV{PATH}")
set(saved_library_path "$ENV{LD_LIBRARY_PATH}")
set(ENV{PATH} "${clean_path}")
if(WIN32)
    set(ENV{LD_LIBRARY_PATH} "")
else()
    set(ENV{LD_LIBRARY_PATH} "${host_build}")
endif()
execute_process(
    COMMAND "${host_executable}" ${host_arguments}
    WORKING_DIRECTORY "${work_dir}"
    RESULT_VARIABLE host_result
    OUTPUT_VARIABLE host_output
    ERROR_VARIABLE host_error)
if(NOT EXISTS "${report_path}")
    message(FATAL_ERROR
        "The reference host produced no run record (exit ${host_result}) running "
        "'${host_executable}'\nstdout:\n${host_output}\nstderr:\n${host_error}")
endif()
file(READ "${report_path}" host_report)
if(NOT host_result EQUAL 0)
    message(FATAL_ERROR
        "The reference host failed with ${host_result}\nrecord:\n${host_report}\n"
        "stderr:\n${host_error}")
endif()

# The record must show the full host lifecycle, not just a successful exit.
foreach(required_event IN ITEMS
        "host.start"
        "host.load source=host-project"
        "host.commit"
        "host.frame index=1 mode=seek"
        "host.digest index=1"
        "host.reload outcome=ok"
        "host.rejection step=reload-failed"
        "host.active .*preserved=yes"
        "host.package outcome=ok"
        "host.destroy state=Empty"
        "host.summary outcome=ok")
    if(NOT host_report MATCHES "${required_event}")
        message(FATAL_ERROR
            "The reference host record is missing '${required_event}'\nrecord:\n${host_report}")
    endif()
endforeach()
if(NOT host_report MATCHES "host.load source=host-project identity=${golden_identity}")
    message(FATAL_ERROR "The reference host did not observe the CFU-F reference identity")
endif()
set(golden_frame_pattern
    "host\\.frame index=1 mode=seek chartTimeMs=625 discontinuityId=1 objects=2 digest=11596562486377158370 algorithm=3")
if(NOT host_report MATCHES "${golden_frame_pattern}")
    message(FATAL_ERROR "The reference host did not observe the CFU-F reference frame digest")
endif()
set(golden_package_pattern
    "host\\.package outcome=ok identity=${golden_identity} identity_matches_host_content=yes frames_identical=yes")
if(NOT host_report MATCHES "${golden_package_pattern}")
    message(FATAL_ERROR "The published package did not reproduce the host content")
endif()

# The sanitized PATH exists only for the host run above; the later configure
# steps need the real toolchain environment back.
set(ENV{PATH} "${saved_path}")
set(ENV{LD_LIBRARY_PATH} "${saved_library_path}")

# No installed file may mention the source tree or the build tree.
file(GLOB_RECURSE installed_headers "${prefix}/include/cuexis/*.hpp")
foreach(header IN LISTS installed_headers)
    file(READ "${header}" header_contents)
    string(FIND "${header_contents}" "${CUEXIS_SOURCE_DIR}" source_hit)
    string(FIND "${header_contents}" "${CUEXIS_BINARY_DIR}" binary_hit)
    if(NOT source_hit EQUAL -1 OR NOT binary_hit EQUAL -1)
        message(FATAL_ERROR "An installed Playback header references the development tree: ${header}")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# 5. Negative gate: a package that records another toolchain is refused.
# ---------------------------------------------------------------------------
if(CUEXIS_LIBRARY_TYPE STREQUAL "SHARED")
    set(doctored_prefix "${work_dir}/doctored-prefix")
    file(COPY "${prefix}/" DESTINATION "${doctored_prefix}")
    set(config_file "${doctored_prefix}/lib/cmake/Cuexis/CuexisConfig.cmake")
    file(READ "${config_file}" config_contents)
    string(REGEX REPLACE
        "set\\(Cuexis_COMPILER_ID \"[^\"]*\"\\)"
        "set(Cuexis_COMPILER_ID \"AnotherCompiler\")"
        config_contents "${config_contents}")
    string(FIND "${config_contents}" "AnotherCompiler" doctor_hit)
    if(doctor_hit EQUAL -1)
        message(FATAL_ERROR "The toolchain rejection case could not doctor the staged package")
    endif()
    file(WRITE "${config_file}" "${config_contents}")

    set(doctored_build "${work_dir}/doctored-build")
    cuexis_host_expect_failure(
        "Reference host configure against a foreign-toolchain package"
        "requires compiler AnotherCompiler"
        "${CMAKE_COMMAND}"
        -S "${host_project}"
        -B "${doctored_build}"
        -G "${CUEXIS_GENERATOR}"
        "-DCMAKE_BUILD_TYPE=${CUEXIS_BUILD_TYPE}"
        "-DCMAKE_CXX_COMPILER=${CUEXIS_CXX_COMPILER}"
        "-DCMAKE_MAKE_PROGRAM=${CUEXIS_MAKE_PROGRAM}"
        "-DCuexis_DIR=${doctored_prefix}/lib/cmake/Cuexis"
        "-DCMAKE_PREFIX_PATH=${doctored_prefix}"
        "-DCUEXIS_HOST_API_VERSION=0.7.0"
        "-DCUEXIS_HOST_CONTENT_DIR=${content_dir}/cfu_f_reference_project"
        ${instrumentation_arguments}
        ${vcpkg_arguments})
    message(STATUS "Reference host refused a foreign-toolchain package")
else()
    message(STATUS
        "Static package: the installed configuration carries no toolchain gate, "
        "so the toolchain rejection case runs only for the shared flavor")
endif()

message(STATUS "Reference host staging verification passed")
