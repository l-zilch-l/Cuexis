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
#
# The sanitized PATH is restored inline right after the run rather than by
# wrapping the whole tail in a function, because the steps below must keep
# emitting their own message(FATAL_ERROR) diagnostics on the caller's stack.
# Restoring immediately after the one execute_process that needs the sanitized
# environment bounds the exposure to that single command (STD-12).
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
# Restore before any check can fail: every message(FATAL_ERROR) from here on
# must leave the caller's process with its real toolchain PATH.
set(ENV{PATH} "${saved_path}")
set(ENV{LD_LIBRARY_PATH} "${saved_library_path}")
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

# ---------------------------------------------------------------------------
# 4b. R9 command-mode cases.
#
# The reference host accepts an external command program, so the transport
# rules a static check cannot reach - pause without an update, resume without
# catch-up, idempotent control, in-place reload - are driven from outside and
# asserted against the run record. This runner drives the same
# installed-package host executable the legacy run above used. It adds no CTest
# test and never nests a ctest invocation.
#
# The fixtures are read from the copied example tree rather than from the
# source tree, so a fixture the whole-directory copy above failed to carry is
# caught here instead of being read from a hidden input.
# ---------------------------------------------------------------------------
set(package_path "${content_dir}/cfu_f_v4_reference.cxc")
set(command_fixture_dir "${host_project}/tests/commands")
include("${CUEXIS_SOURCE_DIR}/cmake/VerifyReferenceHostCommands.cmake")

# The sanitized PATH was restored inline immediately after the one
# execute_process that needed it, so nothing is pending here. The earlier design
# used cmake_language(DEFER), which is unavailable in `cmake -P` script mode.

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
# Candidate isolation: a default (non-candidate) install tree must carry no
# candidate switch or candidate format identifier at all.
#
# The C4 exit report recorded this as a manual scan ("no hits"), which is
# exactly the kind of claim that rots silently. Registering it here makes the
# property a gate instead of a snapshot (SPEC-28).
#
# The scan deliberately matches the five specific tokens rather than the bare
# substring "candidate": the installed public headers legitimately contain
# PresentationCandidateToken and CandidateMetadataAccess for the accepted S5-C
# presentation surface, so a substring scan would fail on a correct tree.
# Verified against a real install before being registered.
set(candidate_isolation_tokens
    "CUEXIS_ENABLE_CHART_V5_CANDIDATE"
    "Cuexis_ALLOW_EXPERIMENTAL"
    "v5.candidate"
    "candidate.1"
    "semantic.v5")
file(GLOB_RECURSE installed_texts
    "${prefix}/include/cuexis/*.hpp"
    "${prefix}/lib/cmake/Cuexis/*.cmake"
    "${prefix}/share/Cuexis/*.txt")
foreach(installed_text IN LISTS installed_texts)
    file(READ "${installed_text}" installed_contents)
    foreach(token IN LISTS candidate_isolation_tokens)
        string(FIND "${installed_contents}" "${token}" token_hit)
        if(NOT token_hit EQUAL -1)
            message(FATAL_ERROR
                "The default install tree exposes the candidate token '${token}': ${installed_text}")
        endif()
    endforeach()
endforeach()
if(NOT installed_texts)
    # An empty scan would make the loop above vacuous and the gate green for the
    # wrong reason, so the absence of any scanned file is itself a failure.
    message(FATAL_ERROR "The candidate isolation scan found no installed files under ${prefix}")
endif()

# ---------------------------------------------------------------------------
# Host import surface: the built host must import only the Cuexis libraries it
# is entitled to. This is the symbol-level half of S6-D08 (SPEC-27): the source
# hygiene check above proves the host *asks* for the right targets, and this
# proves the linker actually produced a binary with the matching dependency set.
#
# The import tool is discovered by the parent build and passed in, because
# find_program() results are not visible inside a -P script. It is deliberately
# a *separate* variable from CUEXIS_SYMBOL_TOOL: the export gate reads the
# dynamic symbol table with `nm`, and this gate reads the import list with
# `objdump`. When it is absent the case is skipped with a notice rather than
# silently passing.
# ---------------------------------------------------------------------------
if(NOT DEFINED CUEXIS_IMPORT_TOOL OR "${CUEXIS_IMPORT_TOOL}" STREQUAL "" OR
   NOT DEFINED CUEXIS_IMPORT_TOOL_KIND OR "${CUEXIS_IMPORT_TOOL_KIND}" STREQUAL "")
    message(STATUS
        "No import tool was provided; the host import surface check was not executed")
else()
    if(CUEXIS_IMPORT_TOOL_KIND STREQUAL "dumpbin")
        execute_process(
            COMMAND "${CUEXIS_IMPORT_TOOL}" /nologo /imports "${host_executable}"
            RESULT_VARIABLE host_import_result
            OUTPUT_VARIABLE host_import_output
            ERROR_VARIABLE host_import_error)
        string(REGEX MATCHALL "[A-Za-z0-9_.+-]+\\.dll" host_imported_libraries
            "${host_import_output}")
    elseif(CUEXIS_IMPORT_TOOL_KIND STREQUAL "objdump")
        # objdump covers both non-MSVC formats, and `nm` cannot serve either:
        #  - PE/COFF (MinGW): the import table is not symbols at all; `nm`
        #    reports "no symbols". objdump lists it as "DLL Name: <name>".
        #  - ELF (Linux): dependencies are DT_NEEDED entries, while `nm -D`
        #    prints symbols (libc_start_main@GLIBC_...) and never the library
        #    file names. objdump lists them as "NEEDED <name>".
        execute_process(
            COMMAND "${CUEXIS_IMPORT_TOOL}" -p "${host_executable}"
            RESULT_VARIABLE host_import_result
            OUTPUT_VARIABLE host_import_output
            ERROR_VARIABLE host_import_error)
        string(REGEX MATCHALL "DLL Name: *([A-Za-z0-9_.+-]+)" host_import_dll_matches
            "${host_import_output}")
        string(REGEX MATCHALL "NEEDED +([A-Za-z0-9_.+-]+)" host_import_needed_matches
            "${host_import_output}")
        set(host_imported_libraries "")
        foreach(import_match IN LISTS host_import_dll_matches)
            string(REGEX REPLACE "^DLL Name: *" "" import_name "${import_match}")
            list(APPEND host_imported_libraries "${import_name}")
        endforeach()
        foreach(import_match IN LISTS host_import_needed_matches)
            string(REGEX REPLACE "^NEEDED +" "" import_name "${import_match}")
            list(APPEND host_imported_libraries "${import_name}")
        endforeach()
    endif()
    if(NOT host_import_result EQUAL 0)
        message(FATAL_ERROR "Host import inspection failed: ${host_import_error}")
    endif()
    if(NOT host_imported_libraries)
        message(FATAL_ERROR
            "The host import inspection parsed no libraries from ${host_executable}; "
            "the check would pass vacuously")
    endif()

    # The host may depend on the Playback SDK plus its own transitive Cuexis
    # runtime, and on nothing else that is Cuexis-owned. Internal modules are
    # named explicitly so a new leak is a build failure rather than a review
    # note. Only libraries whose names start with "cuexis" are judged: the
    # compiler runtime and OS libraries are the host's own business.
    #
    # Names are normalized first, because the two formats spell the same library
    # differently: PE imports are "cuexis_playback-0.7.dll", while ELF sonames
    # are "libcuexis_playback-0.7.so.0.7.0". Without stripping the "lib" prefix
    # and the ".so" suffix chain the ELF names would match neither the
    # ownership guard below nor the forbidden list, which would leave this whole
    # check passing vacuously on Linux while its PE sibling did real work.
    set(host_forbidden_cuexis_libraries
        cuexis_assets
        cuexis_world
        cuexis_runtime
        cuexis_debug
        cuexis_render
        cuexis_render_opengl
        cuexis_platform
        cuexis_behavior
        cuexis_gameplay)
    set(host_cuexis_imports "")
    foreach(imported_library IN LISTS host_imported_libraries)
        string(TOLOWER "${imported_library}" imported_library_lower)
        string(REGEX REPLACE "^lib" "" import_normalized "${imported_library_lower}")
        string(REGEX REPLACE "\\.so.*$" "" import_normalized "${import_normalized}")
        if(NOT import_normalized MATCHES "^cuexis_")
            continue()
        endif()
        list(APPEND host_cuexis_imports "${import_normalized}")
        foreach(forbidden_library IN LISTS host_forbidden_cuexis_libraries)
            if(import_normalized MATCHES "^${forbidden_library}")
                message(FATAL_ERROR
                    "The reference host imports the internal Cuexis library "
                    "${imported_library}; only Playback and its public runtime are allowed")
            endif()
        endforeach()
    endforeach()

    # A static host links the Cuexis objects into the executable, so it imports
    # no Cuexis library at all and the ownership judgement above can never fire.
    # Say so explicitly instead of letting the "verified" line imply otherwise.
    if(CUEXIS_LIBRARY_TYPE STREQUAL "SHARED")
        # A shared package must actually be consumed through its import library;
        # a host that linked nothing would satisfy the forbidden list trivially.
        set(host_imports_playback FALSE)
        foreach(import_normalized IN LISTS host_cuexis_imports)
            if(import_normalized MATCHES "^cuexis_playback")
                set(host_imports_playback TRUE)
            endif()
        endforeach()
        if(NOT host_imports_playback)
            message(FATAL_ERROR
                "The shared reference host does not import cuexis_playback: "
                "${host_imported_libraries}")
        endif()
    elseif(NOT host_cuexis_imports)
        message(STATUS
            "Reference host is static: it imports no Cuexis library, so the "
            "internal-library ownership check has nothing to judge")
    endif()
    message(STATUS "Reference host import surface verified")
endif()

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

# ---------------------------------------------------------------------------
# 6. Negative gate: a host written against an incompatible SDK minor is refused.
#
# The reference host declares CUEXIS_HOST_API_VERSION as the API baseline it was
# written against and refuses anything outside 0.7.x. That refusal was only ever
# checked by hand (C4 report section 3.4), so this case registers it. It runs for
# both flavors: the rejection comes from the installed package configuration's
# own compatibility version, not from a toolchain-specific gate.
#
# The refusal fires inside find_package (examples/reference_host/CMakeLists.txt
# line 23) before the host's explicit 0.7.x guard on line 28 can run, so the
# asserted text is the package-manager message, verified by running the case
# rather than assumed from the guard's wording.
# ---------------------------------------------------------------------------
set(minor_build "${work_dir}/minor-build")
cuexis_host_expect_failure(
    "Reference host configure against an incompatible SDK minor"
    "with requested version \"0\\.8\\.0\""
    "${CMAKE_COMMAND}"
    -S "${host_project}"
    -B "${minor_build}"
    -G "${CUEXIS_GENERATOR}"
    "-DCMAKE_BUILD_TYPE=${CUEXIS_BUILD_TYPE}"
    "-DCMAKE_CXX_COMPILER=${CUEXIS_CXX_COMPILER}"
    "-DCMAKE_MAKE_PROGRAM=${CUEXIS_MAKE_PROGRAM}"
    "-DCuexis_DIR=${prefix}/lib/cmake/Cuexis"
    "-DCMAKE_PREFIX_PATH=${prefix}"
    "-DCUEXIS_HOST_API_VERSION=0.8.0"
    "-DCUEXIS_HOST_CONTENT_DIR=${content_dir}/cfu_f_reference_project"
    ${instrumentation_arguments}
    ${vcpkg_arguments})
message(STATUS "Reference host refused an incompatible SDK minor")

message(STATUS "Reference host staging verification passed")
