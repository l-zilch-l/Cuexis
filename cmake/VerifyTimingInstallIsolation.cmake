# Verifies the RR-F302 installed-prefix shape of the Cuexis static SDK from an already-configured
# build tree.
#
# The script installs the configured tree into a scratch prefix and then checks the prefix itself,
# not the source tree's intent:
#
#   - the default static SDK carries the InternalTiming and InternalJudgement implementation
#     archives, and the experimental static SDK additionally carries InternalChartCodec and
#     InternalChartTools;
#   - no Chart codec/tools, candidate or judgement-compile archive leaks into an installation that
#     does not export it, even when developer tools or the test suite created those targets;
#   - no build-only Chart/timing/judgement header is installed;
#   - every exported INTERFACE_INCLUDE_DIRECTORIES entry stays inside the prefix and every imported
#     archive location is anchored at the prefix, with no BUILD_INTERFACE remnant, source-tree path
#     or dangling archive.
#
# Required:
#   CUEXIS_BINARY_DIR                    build directory to install
#   CUEXIS_SOURCE_DIR                    source root, used to detect source-tree path leaks
#   CUEXIS_ENABLE_CHART_V5_CANDIDATE     ON for the experimental closure, OFF for the ordinary one
# Optional:
#   CUEXIS_LIBRARY_TYPE                  STATIC (default) or SHARED
#   CUEXIS_BUILD_TYPE                    install configuration, passed through as --config
#   CUEXIS_INSTALL_PREFIX                scratch prefix (default under CUEXIS_BINARY_DIR); a
#                                        caller-supplied path must not exist yet
#   CUEXIS_KEEP_INSTALL_PREFIX           ON to keep the scratch prefix after a successful run
#   CUEXIS_REPORT_FILE                   write the check log and installed listing to this file
#
# Usage on an existing static configuration:
#   cmake -DCUEXIS_BINARY_DIR=<absolute build dir> -DCUEXIS_SOURCE_DIR=<absolute source root> \
#         -DCUEXIS_ENABLE_CHART_V5_CANDIDATE=OFF -DCUEXIS_BUILD_TYPE=Debug \
#         -P cmake/VerifyTimingInstallIsolation.cmake
#
# A missing install script, a failing install, a missing expected archive or exported target, an
# installed build-only header, a leaked archive or an include path outside the prefix is itself a
# failure: another configuration's success cannot stand in for this one.

if(NOT DEFINED CUEXIS_BINARY_DIR)
    message(FATAL_ERROR "VerifyTimingInstallIsolation: CUEXIS_BINARY_DIR is required")
endif()
if(NOT DEFINED CUEXIS_SOURCE_DIR)
    message(FATAL_ERROR "VerifyTimingInstallIsolation: CUEXIS_SOURCE_DIR is required")
endif()
if(NOT DEFINED CUEXIS_ENABLE_CHART_V5_CANDIDATE)
    message(FATAL_ERROR "VerifyTimingInstallIsolation: CUEXIS_ENABLE_CHART_V5_CANDIDATE is required")
endif()
if(NOT DEFINED CUEXIS_LIBRARY_TYPE)
    set(CUEXIS_LIBRARY_TYPE STATIC)
endif()
if(NOT DEFINED CUEXIS_BUILD_TYPE)
    set(CUEXIS_BUILD_TYPE "")
endif()
if(NOT DEFINED CUEXIS_KEEP_INSTALL_PREFIX)
    set(CUEXIS_KEEP_INSTALL_PREFIX OFF)
endif()
if(NOT DEFINED CUEXIS_INSTALL_PREFIX)
    string(RANDOM LENGTH 8 ALPHABET 0123456789abcdef install_nonce)
    set(CUEXIS_INSTALL_PREFIX
        "${CUEXIS_BINARY_DIR}/rr-timing-install/${install_nonce}/prefix")
endif()
if(NOT DEFINED CUEXIS_REPORT_FILE)
    set(CUEXIS_REPORT_FILE "")
endif()

if(NOT IS_ABSOLUTE "${CUEXIS_BINARY_DIR}")
    message(FATAL_ERROR
        "VerifyTimingInstallIsolation: CUEXIS_BINARY_DIR must be an absolute path")
endif()
if(NOT IS_ABSOLUTE "${CUEXIS_SOURCE_DIR}")
    message(FATAL_ERROR
        "VerifyTimingInstallIsolation: CUEXIS_SOURCE_DIR must be an absolute path")
endif()

file(TO_CMAKE_PATH "${CUEXIS_BINARY_DIR}" cuexis_binary_root)
file(TO_CMAKE_PATH "${CUEXIS_SOURCE_DIR}" cuexis_source_root)
file(TO_CMAKE_PATH "${CUEXIS_INSTALL_PREFIX}" install_prefix)
string(TOLOWER "${cuexis_binary_root}" cuexis_binary_root_lower)
string(TOLOWER "${install_prefix}" install_prefix_lower)
string(FIND "${install_prefix_lower}/" "${cuexis_binary_root_lower}/" prefix_inside_position)
if(prefix_inside_position EQUAL 0)
    set(install_prefix_managed ON)
else()
    # A caller-supplied prefix outside the build directory is still scratch space, but a stale
    # directory would make "which files exist" ambiguous, so it must be fresh.
    set(install_prefix_managed OFF)
    if(EXISTS "${install_prefix}")
        message(FATAL_ERROR
            "VerifyTimingInstallIsolation: CUEXIS_INSTALL_PREFIX '${install_prefix}' already "
            "exists; remove it or choose a fresh directory")
    endif()
endif()

function(cuexis_report line)
    if(NOT CUEXIS_REPORT_FILE STREQUAL "")
        file(APPEND "${CUEXIS_REPORT_FILE}" "${line}\n")
    endif()
endfunction()

if(NOT CUEXIS_REPORT_FILE STREQUAL "")
    file(WRITE "${CUEXIS_REPORT_FILE}" "")
endif()

file(REMOVE_RECURSE "${install_prefix}")
file(MAKE_DIRECTORY "${install_prefix}")

set(install_script "${cuexis_binary_root}/cmake_install.cmake")
if(NOT EXISTS "${install_script}")
    message(FATAL_ERROR
        "VerifyTimingInstallIsolation: '${install_script}' is missing; the build directory was not "
        "configured with install rules")
endif()
file(READ "${install_script}" install_text)

set(expected_archives cuexis_timing cuexis_judgement)
set(forbidden_archives "")
set(expected_exported_targets Cuexis::InternalTiming Cuexis::InternalJudgement)
set(forbidden_exported_targets "")
if(NOT CUEXIS_LIBRARY_TYPE STREQUAL "STATIC")
    # A shared installation exports public libraries only; it never carries an implementation
    # archive or an internal target.
    set(expected_archives "")
    set(forbidden_archives
        cuexis_timing
        cuexis_judgement
        cuexis_chart_codec
        cuexis_chart_tools
        cuexis_judgement_compile
        cuexis_cxc_candidate
        cuexis_gameplay_packed
    )
    set(expected_exported_targets "")
    set(forbidden_exported_targets
        Cuexis::InternalTiming
        Cuexis::InternalJudgement
        Cuexis::InternalChartCodec
        Cuexis::InternalChartTools
        Cuexis::InternalJudgementCompile
        Cuexis::InternalCxcCandidate
        Cuexis::InternalGameplayPacked
    )
elseif(CUEXIS_ENABLE_CHART_V5_CANDIDATE)
    # The experimental static closure links the Chart support archives, so its installation must
    # export them. The private candidate/Packed implementation archives are part of that closure as
    # well; the judgement_compile archive is intentionally left out of this gate because its
    # experimental installation list is owned by the judgement compile task, not by the Chart split.
    list(APPEND expected_archives cuexis_chart_codec cuexis_chart_tools)
    list(APPEND expected_exported_targets Cuexis::InternalChartCodec Cuexis::InternalChartTools)
else()
    # The default SDK must never grow the Chart support, candidate or compile-side archives, even
    # when developer tools or the test suite created those targets in the same build tree.
    set(forbidden_archives
        cuexis_chart_codec
        cuexis_chart_tools
        cuexis_judgement_compile
        cuexis_cxc_candidate
        cuexis_gameplay_packed
    )
    set(forbidden_exported_targets
        Cuexis::InternalChartCodec
        Cuexis::InternalChartTools
        Cuexis::InternalJudgementCompile
        Cuexis::InternalCxcCandidate
        Cuexis::InternalGameplayPacked
    )
endif()

cuexis_report("# VerifyTimingInstallIsolation")
cuexis_report("binary_dir=${cuexis_binary_root}")
cuexis_report("source_dir=${cuexis_source_root}")
cuexis_report("prefix=${install_prefix}")
cuexis_report("prefix_managed=${install_prefix_managed}")
cuexis_report("library_type=${CUEXIS_LIBRARY_TYPE}")
cuexis_report("chart_candidate=${CUEXIS_ENABLE_CHART_V5_CANDIDATE}")
cuexis_report("build_type=${CUEXIS_BUILD_TYPE}")
cuexis_report("")

if(CUEXIS_LIBRARY_TYPE STREQUAL "STATIC")
    # The install list itself is the first gate: it must name the archives the static consumer
    # closure actually links and must not name any build-only sibling.
    foreach(archive IN LISTS expected_archives)
        string(FIND "${install_text}" "${archive}" install_archive_position)
        if(install_archive_position EQUAL -1)
            message(FATAL_ERROR
                "VerifyTimingInstallIsolation: the install list does not carry the required "
                "implementation archive '${archive}'")
        endif()
        cuexis_report("install-list required archive ${archive}: present")
    endforeach()
    foreach(archive IN LISTS forbidden_archives)
        string(FIND "${install_text}" "${archive}" install_archive_position)
        if(NOT install_archive_position EQUAL -1)
            message(FATAL_ERROR
                "VerifyTimingInstallIsolation: the install list exports the private implementation "
                "archive '${archive}'")
        endif()
        cuexis_report("install-list forbidden archive ${archive}: absent")
    endforeach()
endif()

set(install_command "${CMAKE_COMMAND}" --install "${cuexis_binary_root}" --prefix
    "${install_prefix}" --verbose)
if(NOT CUEXIS_BUILD_TYPE STREQUAL "")
    list(APPEND install_command --config "${CUEXIS_BUILD_TYPE}")
endif()
string(REPLACE ";" " " install_command_text "${install_command}")
cuexis_report("install: ${install_command_text}")
execute_process(
    COMMAND ${install_command}
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
)
cuexis_report("install_exit=${install_result}")
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR
        "VerifyTimingInstallIsolation: installation failed (${install_result})\n"
        "stdout:\n${install_output}\nstderr:\n${install_error}")
endif()
string(REPLACE "\n" "\n# " install_output_lines "${install_output}")
cuexis_report("# install output follows")
cuexis_report("# ${install_output_lines}")
cuexis_report("")

set(package_dirs "${install_prefix}/lib/cmake/Cuexis")
file(GLOB package_dir_matches "${install_prefix}/*/cmake/Cuexis")
if(NOT package_dir_matches)
    message(FATAL_ERROR
        "VerifyTimingInstallIsolation: the installed prefix has no Cuexis CMake package directory")
endif()
list(GET package_dir_matches 0 package_dirs)
get_filename_component(cuexis_lib_dir "${package_dirs}" DIRECTORY)
get_filename_component(cuexis_lib_dir "${cuexis_lib_dir}" DIRECTORY)
cuexis_report("package_dir=${package_dirs}")
cuexis_report("lib_dir=${cuexis_lib_dir}")

function(cuexis_installed_archive_matches name out_matches)
    # One archive name per library, in every supported platform spelling. The install tree has
    # already been normalised by the install rules, so the glob must stay inside the library
    # directory rather than accepting the name anywhere in the prefix.
    file(GLOB matches
        "${cuexis_lib_dir}/${name}.lib"
        "${cuexis_lib_dir}/${name}.a"
        "${cuexis_lib_dir}/lib${name}.a"
        "${cuexis_lib_dir}/lib${name}.dll.a"
        "${cuexis_lib_dir}/${name}.dll"
        "${cuexis_lib_dir}/lib${name}.so"
        "${cuexis_lib_dir}/lib${name}.so.*"
    )
    set(${out_matches} "${matches}" PARENT_SCOPE)
endfunction()

foreach(archive IN LISTS expected_archives)
    cuexis_installed_archive_matches("${archive}" archive_matches)
    if(NOT archive_matches)
        message(FATAL_ERROR
            "VerifyTimingInstallIsolation: the installed prefix is missing the required archive "
            "'${archive}' (looked in '${cuexis_lib_dir}')")
    endif()
    cuexis_report("installed required archive ${archive}: ${archive_matches}")
endforeach()
foreach(archive IN LISTS forbidden_archives)
    cuexis_installed_archive_matches("${archive}" archive_matches)
    if(archive_matches)
        message(FATAL_ERROR
            "VerifyTimingInstallIsolation: the installed prefix carries the private archive "
            "'${archive}': ${archive_matches}")
    endif()
    cuexis_report("installed forbidden archive ${archive}: absent")
endforeach()

set(export_file "${package_dirs}/CuexisTargets.cmake")
file(GLOB export_target_files "${package_dirs}/CuexisTargets*.cmake")
list(SORT export_target_files)
set(package_files
    "${package_dirs}/CuexisConfig.cmake"
    "${package_dirs}/CuexisConfigVersion.cmake"
    ${export_target_files}
)
list(REMOVE_DUPLICATES package_files)
foreach(package_file IN LISTS package_files)
    if(NOT EXISTS "${package_file}")
        message(FATAL_ERROR
            "VerifyTimingInstallIsolation: the installed package is missing '${package_file}'")
    endif()
endforeach()
# Imported locations live in the per-configuration `CuexisTargets-<config>.cmake`; the structural
# checks below read the whole generated export surface, not just the entry file.
set(export_text "")
foreach(export_target_file IN LISTS export_target_files)
    file(READ "${export_target_file}" export_part)
    string(APPEND export_text "${export_part}\n")
endforeach()

foreach(exported IN LISTS expected_exported_targets)
    if(NOT export_text MATCHES "add_library\\(${exported} STATIC IMPORTED\\)")
        message(FATAL_ERROR
            "VerifyTimingInstallIsolation: the installed export does not define '${exported}'")
    endif()
    cuexis_report("exported target ${exported}: present")
endforeach()
foreach(exported IN LISTS forbidden_exported_targets)
    if(export_text MATCHES "add_library\\(${exported} STATIC IMPORTED\\)")
        message(FATAL_ERROR
            "VerifyTimingInstallIsolation: the installed export defines the private target "
            "'${exported}'")
    endif()
    cuexis_report("exported target ${exported}: absent")
endforeach()

# Build-only headers: the codec/tools include roots and the Judgement compile root are
# BUILD_INTERFACE-only, so none of their headers may appear in the prefix.
set(forbidden_header_directories
    "${install_prefix}/include/cuexis/timing"
    "${install_prefix}/include/cuexis/judgement"
    "${install_prefix}/include/cuexis/chart/internal"
)
foreach(forbidden_directory IN LISTS forbidden_header_directories)
    if(EXISTS "${forbidden_directory}")
        message(FATAL_ERROR
            "VerifyTimingInstallIsolation: the installed prefix contains the build-only header "
            "directory '${forbidden_directory}'")
    endif()
endforeach()
file(GLOB internal_header_directories "${install_prefix}/include/cuexis/*/internal")
if(internal_header_directories)
    message(FATAL_ERROR
        "VerifyTimingInstallIsolation: the installed prefix contains build-only internal header "
        "directories: ${internal_header_directories}")
endif()
set(forbidden_headers
    chart/canonical_semantic_chart.hpp
    chart/candidate_assembler.hpp
    chart/candidate_lowering.hpp
    chart/behavior_candidate.hpp
    chart/chart_migrator.hpp
    chart/chart_v5_document.hpp
    chart/chart_v5_timing.hpp
    chart/compiled_chart_timing.hpp
    chart/cxt_v2_loader.hpp
    chart/packed_chart_io.hpp
    chart/packed_chart_primitives.hpp
    chart/packed_chart_reader.hpp
    chart/packed_chart_tables.hpp
    chart/packed_chart_writer.hpp
)
foreach(forbidden_header IN LISTS forbidden_headers)
    if(EXISTS "${install_prefix}/include/cuexis/${forbidden_header}")
        message(FATAL_ERROR
            "VerifyTimingInstallIsolation: the installed prefix contains the build-only header "
            "'cuexis/${forbidden_header}'")
    endif()
endforeach()
cuexis_report("build-only header checks: absent")
cuexis_report("")

foreach(package_file IN LISTS package_files)
    file(READ "${package_file}" package_contents)
    string(REPLACE "\\" "/" package_contents_normalized "${package_contents}")
    string(TOLOWER "${package_contents_normalized}" package_contents_normalized)
    if(package_contents_normalized MATCHES "build_interface")
        message(FATAL_ERROR
            "VerifyTimingInstallIsolation: '${package_file}' still carries a BUILD_INTERFACE "
            "expression")
    endif()
    string(TOLOWER "${cuexis_source_root}" cuexis_source_root_lower)
    string(REPLACE "\\" "/" cuexis_source_root_lower "${cuexis_source_root_lower}")
    string(FIND "${package_contents_normalized}" "${cuexis_source_root_lower}" source_leak_position)
    if(NOT source_leak_position EQUAL -1)
        message(FATAL_ERROR
            "VerifyTimingInstallIsolation: '${package_file}' leaks the source tree path "
            "'${cuexis_source_root}'")
    endif()
    string(REGEX MATCHALL "[A-Za-z]:[\\/][^\"\r\n\t;]*" absolute_paths "${package_contents}")
    if(absolute_paths)
        message(FATAL_ERROR
            "VerifyTimingInstallIsolation: '${package_file}' carries an absolute path: "
            "${absolute_paths}")
    endif()
endforeach()
cuexis_report("package files checked: ${package_files}")
cuexis_report("")

# Installed consumers must see include directories only inside the prefix, and every exported
# archive must resolve to a file that the installation actually created.
string(REGEX MATCHALL "INTERFACE_INCLUDE_DIRECTORIES[ \t\r\n]+\"([^\"]*)\"" include_entries
    "${export_text}")
foreach(entry IN LISTS include_entries)
    string(REGEX REPLACE ".*\"([^\"]*)\"$" "\\1" include_path "${entry}")
    if(include_path STREQUAL "")
        continue()
    endif()
    if(include_path MATCHES "^\\$\\{_IMPORT_PREFIX\\}")
        cuexis_report("exported include dir ${include_path}: inside prefix")
        continue()
    endif()
    if(NOT include_path MATCHES "^/")
        cuexis_report("exported include dir ${include_path}: relative")
        continue()
    endif()
    message(FATAL_ERROR
        "VerifyTimingInstallIsolation: the installed export exposes an include directory outside "
        "the prefix: '${include_path}'")
endforeach()

string(REGEX MATCHALL "IMPORTED_LOCATION[A-Z_]*[ \t]+\"([^\"]*)\"" location_entries "${export_text}")
if(NOT location_entries)
    message(FATAL_ERROR
        "VerifyTimingInstallIsolation: the installed export carries no imported archive location")
endif()
foreach(entry IN LISTS location_entries)
    string(REGEX REPLACE ".*\"([^\"]*)\"$" "\\1" location_path "${entry}")
    if(NOT location_path MATCHES "^\\$\\{_IMPORT_PREFIX\\}/")
        message(FATAL_ERROR
            "VerifyTimingInstallIsolation: the installed export anchors an archive outside the "
            "prefix: '${location_path}'")
    endif()
    string(REPLACE "\${_IMPORT_PREFIX}" "${install_prefix}" resolved_path "${location_path}")
    if(NOT EXISTS "${resolved_path}")
        message(FATAL_ERROR
            "VerifyTimingInstallIsolation: the installed export points at a missing archive "
            "'${resolved_path}'")
    endif()
endforeach()
cuexis_report("imported archive locations: all present inside the prefix")
cuexis_report("")

file(GLOB installed_archives "${cuexis_lib_dir}/*")
list(SORT installed_archives)
foreach(installed_archive IN LISTS installed_archives)
    if(IS_DIRECTORY "${installed_archive}")
        continue()
    endif()
    get_filename_component(installed_archive_name "${installed_archive}" NAME)
    cuexis_report("installed lib/${installed_archive_name}")
endforeach()

if(CUEXIS_KEEP_INSTALL_PREFIX)
    cuexis_report("")
    cuexis_report("prefix kept at ${install_prefix}")
else()
    file(REMOVE_RECURSE "${install_prefix}")
    cuexis_report("")
    cuexis_report("prefix removed after verification")
endif()

message(STATUS
    "VerifyTimingInstallIsolation: ${CUEXIS_ENABLE_CHART_V5_CANDIDATE} static prefix OK "
    "(library type ${CUEXIS_LIBRARY_TYPE}; archives: ${expected_archives})")
