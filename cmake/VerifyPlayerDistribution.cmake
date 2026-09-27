# Verifies the runnable Cuexis Player distribution (ADR 0042 S6-D08, stage plan
# S6-C4 item 3).
#
# The gate packages the Player, then proves that the packaged directory is
# self-contained and reproducible:
#   * the documented packaging target produces one flavor-labelled directory,
#   * the directory carries the executable, its runtime libraries, the default
#     resource location, version metadata and the complete license set,
#   * it contains no build-only artifact and no reference to the development
#     tree,
#   * the packaged Player starts with a sanitized PATH from a copied directory
#     and reports stable diagnostic codes for rejected arguments and unreadable
#     content,
#   * the SDK install tree and the Player distribution stay separate.
#
# Required variables: CUEXIS_SOURCE_DIR, CUEXIS_BINARY_DIR, CUEXIS_GENERATOR,
# CUEXIS_BUILD_TYPE, CUEXIS_LIBRARY_TYPE, CUEXIS_VERSION_DISPLAY.

foreach(required IN ITEMS
        CUEXIS_SOURCE_DIR
        CUEXIS_BINARY_DIR
        CUEXIS_GENERATOR
        CUEXIS_BUILD_TYPE
        CUEXIS_LIBRARY_TYPE
        CUEXIS_VERSION_DISPLAY)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

set(work_dir "${CUEXIS_BINARY_DIR}/ec/player-dist")
file(REMOVE_RECURSE "${work_dir}")
file(MAKE_DIRECTORY "${work_dir}")

function(cuexis_dist_run_checked label)
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

# ---------------------------------------------------------------------------
# 1. Package through the documented target.
# ---------------------------------------------------------------------------
cuexis_dist_run_checked(
    "Player distribution packaging"
    "${CMAKE_COMMAND}" --build "${CUEXIS_BINARY_DIR}" --target cuexis_player_dist)

set(dist_record "${CUEXIS_BINARY_DIR}/dist/cuexis-player-dist.txt")
if(NOT EXISTS "${dist_record}")
    message(FATAL_ERROR "The packaging target produced no record at ${dist_record}")
endif()
file(READ "${dist_record}" dist_record_contents)
if(NOT dist_record_contents MATCHES "path=([^\n]+)")
    message(FATAL_ERROR "The packaging record has no path:\n${dist_record_contents}")
endif()
set(dist_dir "${CMAKE_MATCH_1}")
string(STRIP "${dist_dir}" dist_dir)
if(NOT IS_DIRECTORY "${dist_dir}")
    message(FATAL_ERROR "The recorded distribution directory does not exist: ${dist_dir}")
endif()
if(NOT dist_record_contents MATCHES "flavor=${CUEXIS_LIBRARY_TYPE}-${CUEXIS_BUILD_TYPE}")
    message(FATAL_ERROR
        "The distribution flavor does not match this build "
        "(${CUEXIS_LIBRARY_TYPE}-${CUEXIS_BUILD_TYPE}):\n${dist_record_contents}")
endif()

# ---------------------------------------------------------------------------
# 2. Contents: executable, resources, metadata, licenses.
# ---------------------------------------------------------------------------
set(player_executable "")
foreach(candidate IN ITEMS
        "${dist_dir}/cuexis_player.exe"
        "${dist_dir}/cuexis_player")
    if(EXISTS "${candidate}")
        set(player_executable "${candidate}")
        break()
    endif()
endforeach()
if(player_executable STREQUAL "")
    message(FATAL_ERROR "The distribution does not contain the Player executable")
endif()

foreach(required_resource IN ITEMS
        assets/schemas/cuexis.player-preferences.v1.schema.json
        assets/schemas/cuexis.audio-device-profile.v1.schema.json
        assets/projects/stage1d_project
        assets/projects/stage3_project
        assets/charts/stage1a_example.cuexis.chart.json
        VERSION.txt
        README.txt
        LICENSE
        NOTICE
        THIRD_PARTY_NOTICES.md)
    if(NOT EXISTS "${dist_dir}/${required_resource}")
        message(FATAL_ERROR "The distribution is missing ${required_resource}")
    endif()
endforeach()

foreach(metadata IN ITEMS
        "format=cuexis.player-distribution"
        "display_version=${CUEXIS_VERSION_DISPLAY}"
        "sdk_api_version=0.7.0"
        "library_type=${CUEXIS_LIBRARY_TYPE}"
        "build_type=${CUEXIS_BUILD_TYPE}"
        "resources=assets")
    file(READ "${dist_dir}/VERSION.txt" version_contents)
    if(NOT version_contents MATCHES "${metadata}")
        message(FATAL_ERROR
            "The distribution metadata is missing '${metadata}':\n${version_contents}")
    endif()
endforeach()

# A distribution never ships build-only artifacts.
file(GLOB_RECURSE build_only_artifacts
    "${dist_dir}/*.lib"
    "${dist_dir}/*.pdb"
    "${dist_dir}/*.ilk"
    "${dist_dir}/*.exp"
    "${dist_dir}/CMakeCache.txt"
    "${dist_dir}/build.ninja"
    "${dist_dir}/cuexis-player-dist.txt")
if(build_only_artifacts)
    message(FATAL_ERROR "The distribution ships build-only artifacts: ${build_only_artifacts}")
endif()

# Required license set: the project notices plus every redistributed port.
set(required_licenses
    entt fmt glad glm json-schema-validator minizip-ng nlohmann-json sdl3
    spdlog tl-expected)
foreach(license IN LISTS required_licenses)
    set(license_file "${dist_dir}/licenses/${license}-copyright.txt")
    if(NOT EXISTS "${license_file}")
        message(FATAL_ERROR "The distribution is missing the ${license} license text")
    endif()
    file(SIZE "${license_file}" license_size)
    if(license_size LESS 64)
        message(FATAL_ERROR "The ${license} license text is empty")
    endif()
endforeach()
file(GLOB distribution_licenses "${dist_dir}/licenses/*.txt")
list(LENGTH distribution_licenses distribution_license_count)
list(LENGTH required_licenses required_license_count)
if(NOT distribution_license_count EQUAL required_license_count)
    message(FATAL_ERROR
        "The distribution license set differs from the review list: "
        "${distribution_licenses}")
endif()

# The distribution is not an SDK install tree.
if(EXISTS "${dist_dir}/lib/cmake/Cuexis/CuexisConfig.cmake" OR
   EXISTS "${dist_dir}/include/cuexis/playback")
    message(FATAL_ERROR "The Player distribution must not contain the SDK install tree")
endif()

# No packaged text may name the development tree.
file(GLOB_RECURSE distribution_texts "${dist_dir}/*.txt" "${dist_dir}/*.md")
foreach(text IN LISTS distribution_texts)
    file(READ "${text}" text_contents)
    string(FIND "${text_contents}" "${CUEXIS_SOURCE_DIR}" source_hit)
    string(FIND "${text_contents}" "${CUEXIS_BINARY_DIR}" binary_hit)
    if(NOT source_hit EQUAL -1 OR NOT binary_hit EQUAL -1)
        message(FATAL_ERROR "A packaged file references the development tree: ${text}")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# 3. Runtime deployment: start the copied distribution with a sanitized PATH.
# ---------------------------------------------------------------------------
set(dist_copy "${work_dir}/dist-copy")
file(COPY "${dist_dir}/" DESTINATION "${dist_copy}")

if(WIN32)
    set(clean_path "${dist_copy};$ENV{SystemRoot}/System32;$ENV{SystemRoot}")
else()
    set(clean_path "${dist_copy}:/usr/bin:/bin")
endif()

function(cuexis_dist_expect_failure label expected_code)
    execute_process(
        COMMAND "${dist_copy}/cuexis_player.exe" ${ARGN}
        WORKING_DIRECTORY "${dist_copy}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error)
    set(combined "${output}\n${error}")
    if(result EQUAL 0)
        message(FATAL_ERROR "${label} was accepted but had to be refused")
    endif()
    if(NOT combined MATCHES "${expected_code}")
        message(FATAL_ERROR
            "${label} did not report '${expected_code}'\nstdout:\n${output}\nstderr:\n${error}")
    endif()
    if(combined MATCHES "0xc0000135" OR combined MATCHES "0xC0000135" OR
       combined MATCHES "not found while loading")
        message(FATAL_ERROR "${label} could not load a runtime library:\n${combined}")
    endif()
endfunction()

set(ENV{PATH} "${clean_path}")
execute_process(
    COMMAND "${dist_copy}/cuexis_player.exe" --chart "missing-chart.json"
    WORKING_DIRECTORY "${dist_copy}"
    RESULT_VARIABLE chart_result
    OUTPUT_VARIABLE chart_output
    ERROR_VARIABLE chart_error)
set(chart_combined "${chart_output}\n${chart_error}")
if(NOT chart_result EQUAL 0)
    if(chart_combined MATCHES "player.chart.open_failed")
        message(STATUS "Packaged Player refused unreadable content with player.chart.open_failed")
    else()
        message(FATAL_ERROR
            "The packaged Player failed without a stable diagnostic:\n${chart_combined}")
    endif()
else()
    message(FATAL_ERROR "The packaged Player accepted a missing chart")
endif()

cuexis_dist_expect_failure("Packaged Player unknown argument" "player.arguments.unknown" "--bogus")
cuexis_dist_expect_failure(
    "Packaged Player missing chart argument" "player.arguments.chart_path_missing" "--chart")

message(STATUS "Player distribution verification passed: ${dist_dir}")
