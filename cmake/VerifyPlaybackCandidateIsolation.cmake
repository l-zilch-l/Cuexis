# Verifies the R1 Playback candidate build isolation and the RR-F302 Chart source split from an
# already-configured build tree.
#
# The script reads the CMake File API codemodel that configure produced. It does not configure,
# build or install anything, and it never trusts the source tree's intent: it classifies the
# actual compile sources, include roots and target dependencies recorded for the selected
# configuration.
#
# Required:
#   CUEXIS_BINARY_DIR                    build directory of the configuration to inspect
#   CUEXIS_ENABLE_CHART_V5_CANDIDATE      ON for the experimental closure, OFF for the ordinary one
# Optional:
#   CUEXIS_PLAYER_TARGET                  ON when the cuexis_player target exists
#   CUEXIS_LIBRARY_TYPE                   STATIC (default) or SHARED
#   CUEXIS_BUILD_DEVELOPER_TOOLS          ON when developer tools were configured
#   CUEXIS_BUILD_TESTS                    ON when BUILD_TESTING and CUEXIS_BUILD_TESTS created the
#                                         test support libraries (default OFF)
#   CUEXIS_SOURCE_DIR                     source root, used only for readable diagnostics
#
# Usage on an existing configuration:
#   cmake -DCUEXIS_BINARY_DIR=out/build/headless-debug -DCUEXIS_ENABLE_CHART_V5_CANDIDATE=OFF
#         -DCUEXIS_PLAYER_TARGET=OFF -P cmake/VerifyPlaybackCandidateIsolation.cmake
#
# The Chart codec/tools libraries exist under the shared support condition
# (candidate OR developer tools OR enabled tests). When the condition is false the script
# requires their targets and translation units to be absent; when it is true it requires each
# Chart translation unit to belong to exactly one of cuexis_chart/chart_codec/chart_tools.
#
# A missing codemodel reply, a missing expected target, a missing or unexpected Chart source or
# include root, or a candidate source leaking into the ordinary closure is itself a failure:
# another configuration's success cannot stand in for this one.

if(NOT DEFINED CUEXIS_BINARY_DIR)
    message(FATAL_ERROR
        "VerifyPlaybackCandidateIsolation: CUEXIS_BINARY_DIR is required")
endif()
if(NOT DEFINED CUEXIS_ENABLE_CHART_V5_CANDIDATE)
    message(FATAL_ERROR
        "VerifyPlaybackCandidateIsolation: CUEXIS_ENABLE_CHART_V5_CANDIDATE is required")
endif()
if(NOT DEFINED CUEXIS_PLAYER_TARGET)
    set(CUEXIS_PLAYER_TARGET OFF)
endif()
if(NOT DEFINED CUEXIS_LIBRARY_TYPE)
    set(CUEXIS_LIBRARY_TYPE STATIC)
endif()
if(NOT DEFINED CUEXIS_BUILD_DEVELOPER_TOOLS)
    set(CUEXIS_BUILD_DEVELOPER_TOOLS OFF)
endif()
if(NOT DEFINED CUEXIS_BUILD_TESTS)
    set(CUEXIS_BUILD_TESTS OFF)
endif()
if(NOT DEFINED CUEXIS_SOURCE_DIR)
    set(CUEXIS_SOURCE_DIR "")
endif()

set(reply_dir "${CUEXIS_BINARY_DIR}/.cmake/api/v1/reply")
file(GLOB codemodel_files "${reply_dir}/codemodel-v2-*.json")
if(NOT codemodel_files)
    message(FATAL_ERROR
        "VerifyPlaybackCandidateIsolation: no File API codemodel in '${reply_dir}'. "
        "Place an empty 'codemodel-v2' file in "
        "'${CUEXIS_BINARY_DIR}/.cmake/api/v1/query' before configuring.")
endif()
list(SORT codemodel_files)
list(GET codemodel_files 0 codemodel_file)
file(READ "${codemodel_file}" codemodel)

if(NOT CUEXIS_SOURCE_DIR)
    # Manual invocations may pass only the build directory; the codemodel records the matching
    # source root, so the Chart include-root checks do not depend on the caller repeating it.
    string(JSON CUEXIS_SOURCE_DIR GET "${codemodel}" paths source)
endif()

string(JSON codemodel_configuration_count ERROR_VARIABLE codemodel_error
    LENGTH "${codemodel}" configurations)
if(codemodel_error OR codemodel_configuration_count LESS 1)
    message(FATAL_ERROR
        "VerifyPlaybackCandidateIsolation: codemodel '${codemodel_file}' has no configuration: "
        "${codemodel_error}")
endif()

# The repository uses single-config Ninja presets; inspecting the first configuration reflects
# the requested build. A multi-config codemodel would make the source closure ambiguous, so fail
# instead of silently checking only one of the configurations.
if(codemodel_configuration_count GREATER 1)
    message(FATAL_ERROR
        "VerifyPlaybackCandidateIsolation: expected a single-configuration codemodel, got "
        "${codemodel_configuration_count}. Inspect one preset binary directory per configuration.")
endif()

string(JSON target_count ERROR_VARIABLE target_error
    LENGTH "${codemodel}" configurations 0 targets)
if(target_error OR target_count LESS 1)
    message(FATAL_ERROR
        "VerifyPlaybackCandidateIsolation: codemodel contains no targets: ${target_error}")
endif()

set(target_names "")
set(target_json_files "")
set(target_ids "")
set(index 0)
while(index LESS target_count)
    string(JSON name GET "${codemodel}" configurations 0 targets ${index} name)
    string(JSON json_file GET "${codemodel}" configurations 0 targets ${index} jsonFile)
    string(JSON target_id ERROR_VARIABLE target_id_error
        GET "${codemodel}" configurations 0 targets ${index} id)
    if(target_id_error)
        # Older codemodel minor versions may omit the id at the index level; the target JSON
        # still carries it, so fall back to a stable placeholder and resolve from there.
        set(target_id "index-${index}")
    endif()
    list(APPEND target_names "${name}")
    list(APPEND target_json_files "${json_file}")
    list(APPEND target_ids "${target_id}")
    math(EXPR index "${index} + 1")
endwhile()

function(cuexis_target_json target out_json)
    list(FIND target_names "${target}" position)
    if(position EQUAL -1)
        set(${out_json} "" PARENT_SCOPE)
        return()
    endif()
    list(GET target_json_files ${position} current_json)
    if(NOT IS_ABSOLUTE "${current_json}")
        set(current_json "${reply_dir}/${current_json}")
    endif()
    set(${out_json} "${current_json}" PARENT_SCOPE)
endfunction()

function(cuexis_target_cpp_sources target out_sources)
    cuexis_target_json("${target}" current_json)
    set(result "")
    if(NOT current_json)
        set(${out_sources} "" PARENT_SCOPE)
        return()
    endif()
    file(READ "${current_json}" target_json)
    string(JSON source_count ERROR_VARIABLE source_error
        LENGTH "${target_json}" sources)
    if(source_error)
        set(${out_sources} "" PARENT_SCOPE)
        return()
    endif()
    set(source_index 0)
    while(source_index LESS source_count)
        string(JSON source_path GET "${target_json}" sources ${source_index} path)
        if(source_path MATCHES "\\.cpp$")
            get_filename_component(source_name "${source_path}" NAME)
            list(APPEND result "${source_name}")
        endif()
        math(EXPR source_index "${source_index} + 1")
    endwhile()
    set(${out_sources} "${result}" PARENT_SCOPE)
endfunction()

function(cuexis_target_include_roots target out_roots)
    cuexis_target_json("${target}" current_json)
    set(result "")
    if(NOT current_json)
        set(${out_roots} "" PARENT_SCOPE)
        return()
    endif()
    file(READ "${current_json}" target_json)
    string(JSON group_count ERROR_VARIABLE group_error
        LENGTH "${target_json}" compileGroups)
    if(group_error)
        set(${out_roots} "" PARENT_SCOPE)
        return()
    endif()
    set(group_index 0)
    while(group_index LESS group_count)
        string(JSON include_count ERROR_VARIABLE include_error
            LENGTH "${target_json}" compileGroups ${group_index} includes)
        if(NOT include_error)
            set(include_index 0)
            while(include_index LESS include_count)
                string(JSON include_path GET "${target_json}" compileGroups ${group_index}
                    includes ${include_index} path)
                file(TO_CMAKE_PATH "${include_path}" normalized_path)
                list(APPEND result "${normalized_path}")
                math(EXPR include_index "${include_index} + 1")
            endwhile()
        endif()
        math(EXPR group_index "${group_index} + 1")
    endwhile()
    set(${out_roots} "${result}" PARENT_SCOPE)
endfunction()

function(cuexis_target_dependency_names target out_names)
    cuexis_target_json("${target}" current_json)
    set(result "")
    if(NOT current_json)
        set(${out_names} "" PARENT_SCOPE)
        return()
    endif()
    file(READ "${current_json}" target_json)
    string(JSON dependency_count ERROR_VARIABLE dependency_error
        LENGTH "${target_json}" dependencies)
    if(dependency_error)
        set(${out_names} "" PARENT_SCOPE)
        return()
    endif()
    set(dependency_index 0)
    while(dependency_index LESS dependency_count)
        string(JSON dependency_id GET "${target_json}" dependencies ${dependency_index} id)
        list(FIND target_ids "${dependency_id}" dependency_position)
        if(NOT dependency_position EQUAL -1)
            list(GET target_names ${dependency_position} dependency_name)
            list(APPEND result "${dependency_name}")
        endif()
        math(EXPR dependency_index "${dependency_index} + 1")
    endwhile()
    set(${out_names} "${result}" PARENT_SCOPE)
endfunction()

function(cuexis_target_closure root reply_dir names ids json_files out_targets
         out_source_files out_source_targets)
    set(queue "${root}")
    set(seen_targets "")
    set(source_files "")
    set(source_targets "")
    while(queue)
        list(POP_FRONT queue current)
        if(current IN_LIST seen_targets)
            continue()
        endif()
        list(FIND names "${current}" position)
        if(position EQUAL -1)
            # Imported or external targets are not part of the codemodel target table. They
            # carry no Cuexis candidate sources, so the closure continues without them.
            list(APPEND seen_targets "${current}")
            continue()
        endif()
        list(APPEND seen_targets "${current}")
        list(GET json_files ${position} current_json)
        if(NOT IS_ABSOLUTE "${current_json}")
            set(current_json "${reply_dir}/${current_json}")
        endif()
        file(READ "${current_json}" target_json)
        # Link dependencies live in the target object, as an array of target ids. The codemodel
        # index only names the target objects, so the ids are resolved back through the index.
        string(JSON dependency_count ERROR_VARIABLE dependency_error
            LENGTH "${target_json}" dependencies)
        if(NOT dependency_error)
            set(dependency_index 0)
            while(dependency_index LESS dependency_count)
                string(JSON dependency_id GET "${target_json}" dependencies ${dependency_index} id)
                list(FIND ids "${dependency_id}" dependency_position)
                if(NOT dependency_position EQUAL -1)
                    list(GET names ${dependency_position} dependency_name)
                    list(APPEND queue "${dependency_name}")
                endif()
                math(EXPR dependency_index "${dependency_index} + 1")
            endwhile()
        endif()
        string(JSON source_count ERROR_VARIABLE source_error
            LENGTH "${target_json}" sources)
        if(source_error)
            continue()
        endif()
        set(source_index 0)
        while(source_index LESS source_count)
            string(JSON source_path GET "${target_json}" sources ${source_index} path)
            get_filename_component(source_name "${source_path}" NAME)
            list(APPEND source_files "${source_name}")
            list(APPEND source_targets "${current}")
            math(EXPR source_index "${source_index} + 1")
        endwhile()
    endwhile()
    set(${out_targets} "${seen_targets}" PARENT_SCOPE)
    set(${out_source_files} "${source_files}" PARENT_SCOPE)
    set(${out_source_targets} "${source_targets}" PARENT_SCOPE)
endfunction()

function(cuexis_assert_cpp_sources target expected)
    cuexis_target_cpp_sources("${target}" actual)
    foreach(required IN LISTS expected)
        if(NOT required IN_LIST actual)
            message(FATAL_ERROR
                "VerifyPlaybackCandidateIsolation: Chart target '${target}' does not compile the "
                "expected translation unit '${required}'")
        endif()
    endforeach()
    foreach(source IN LISTS actual)
        if(NOT source IN_LIST expected)
            message(FATAL_ERROR
                "VerifyPlaybackCandidateIsolation: Chart target '${target}' compiles the "
                "unexpected translation unit '${source}'; each Chart TU belongs to exactly one "
                "of cuexis_chart, cuexis_chart_codec and cuexis_chart_tools")
        endif()
    endforeach()
endfunction()

function(cuexis_assert_chart_owner source expected_owner pairs)
    set(owners "")
    foreach(pair IN LISTS pairs)
        string(REPLACE "|" ";" parts "${pair}")
        list(GET parts 0 pair_source)
        list(GET parts 1 pair_target)
        if(pair_source STREQUAL "${source}")
            list(APPEND owners "${pair_target}")
        endif()
    endforeach()
    list(LENGTH owners owner_count)
    if(NOT owner_count EQUAL 1)
        message(FATAL_ERROR
            "VerifyPlaybackCandidateIsolation: Chart translation unit '${source}' must be "
            "compiled by exactly one target, but ${owner_count} targets compile it (${owners})")
    endif()
    if(NOT owners STREQUAL "${expected_owner}")
        message(FATAL_ERROR
            "VerifyPlaybackCandidateIsolation: Chart translation unit '${source}' belongs to "
            "'${expected_owner}' but is compiled by '${owners}'")
    endif()
endfunction()

function(cuexis_assert_include_root target expected include_roots)
    if(NOT expected IN_LIST include_roots)
        message(FATAL_ERROR
            "VerifyPlaybackCandidateIsolation: Chart target '${target}' is missing its expected "
            "include root '${expected}' (actual: ${include_roots})")
    endif()
endfunction()

function(cuexis_assert_no_include_root target forbidden include_roots)
    if(forbidden IN_LIST include_roots)
        message(FATAL_ERROR
            "VerifyPlaybackCandidateIsolation: target '${target}' exposes the forbidden "
            "include root '${forbidden}'")
    endif()
endfunction()

set(candidate_source_names
    playback_session_execution.cpp
    playback_session_gameplay.cpp
    playback_session_gameplay_seek.cpp
    playback_session_prepare_gameplay.cpp
    playback_session_realtime.cpp
    playback_source_gameplay.cpp
    gameplay_candidate.cpp
    gameplay_configuration.cpp
    gameplay_entry.cpp
    presentation_projection.cpp
    cxc_candidate.cpp
)
set(candidate_target_names
    cuexis_cxc_candidate
    cuexis_judgement
    cuexis_gameplay_packed
    cuexis_gameplay_packed_read
    cuexis_gameplay_packed_source
    cuexis_gameplay_author
    cuexis_gameplay_author_cli
    cuexis_judgement_compile
    cuexis_chart_codec
    cuexis_chart_tools
)

list(FIND target_names cuexis_playback playback_position)
if(playback_position EQUAL -1)
    message(FATAL_ERROR
        "VerifyPlaybackCandidateIsolation: codemodel in '${CUEXIS_BINARY_DIR}' has no "
        "cuexis_playback target")
endif()

cuexis_target_closure(cuexis_playback "${reply_dir}" "${target_names}" "${target_ids}"
    "${target_json_files}" playback_targets playback_sources playback_source_targets)

set(inspected_targets cuexis_playback)
if(CUEXIS_PLAYER_TARGET)
    list(FIND target_names cuexis_player player_position)
    if(player_position EQUAL -1)
        message(FATAL_ERROR
            "VerifyPlaybackCandidateIsolation: CUEXIS_PLAYER_TARGET is ON but the codemodel "
            "has no cuexis_player target")
    endif()
    cuexis_target_closure(cuexis_player "${reply_dir}" "${target_names}" "${target_ids}"
        "${target_json_files}" player_targets player_sources player_source_targets)
    list(APPEND inspected_targets cuexis_player)
else()
    set(player_targets "")
    set(player_sources "")
    set(player_source_targets "")
endif()

foreach(inspected IN LISTS inspected_targets)
    if(inspected STREQUAL "cuexis_playback")
        set(current_targets ${playback_targets})
        set(current_sources ${playback_sources})
        set(current_source_targets ${playback_source_targets})
    else()
        set(current_targets ${player_targets})
        set(current_sources ${player_sources})
        set(current_source_targets ${player_source_targets})
    endif()

    if(CUEXIS_ENABLE_CHART_V5_CANDIDATE)
        foreach(required IN LISTS candidate_source_names)
            if(required STREQUAL "cxc_candidate.cpp")
                continue()
            endif()
            list(FIND current_sources "${required}" required_position)
            if(required_position EQUAL -1)
                message(FATAL_ERROR
                    "VerifyPlaybackCandidateIsolation: experimental ${inspected} closure does "
                    "not compile the expected candidate source '${required}'")
            endif()
            # Each hook owner is compiled exactly once in the experimental closure; a duplicate
            # source entry would define the same hook twice.
            set(match_count 0)
            foreach(source_entry IN LISTS current_sources)
                if(source_entry STREQUAL "${required}")
                    math(EXPR match_count "${match_count} + 1")
                endif()
            endforeach()
            if(match_count GREATER 1)
                message(FATAL_ERROR
                    "VerifyPlaybackCandidateIsolation: experimental ${inspected} closure "
                    "compiles candidate source '${required}' ${match_count} times")
            endif()
        endforeach()
        if(inspected STREQUAL "cuexis_playback")
            foreach(required_target cuexis_cxc_candidate cuexis_chart_codec cuexis_chart_tools)
                list(FIND current_targets "${required_target}" required_target_position)
                if(required_target_position EQUAL -1)
                    message(FATAL_ERROR
                        "VerifyPlaybackCandidateIsolation: experimental cuexis_playback closure "
                        "does not depend on ${required_target}")
                endif()
            endforeach()
            list(FIND current_sources "cxc_candidate.cpp" cxc_candidate_position)
            if(cxc_candidate_position EQUAL -1)
                message(FATAL_ERROR
                    "VerifyPlaybackCandidateIsolation: experimental cuexis_playback closure does "
                    "not compile cxc_candidate.cpp through cuexis_cxc_candidate")
            endif()
        endif()
    else()
        foreach(forbidden IN LISTS candidate_source_names)
            list(FIND current_sources "${forbidden}" forbidden_position)
            if(NOT forbidden_position EQUAL -1)
                list(GET current_source_targets ${forbidden_position} owner)
                message(FATAL_ERROR
                    "VerifyPlaybackCandidateIsolation: ordinary ${inspected} closure compiles "
                    "candidate source '${forbidden}' in target '${owner}'")
            endif()
        endforeach()
        foreach(forbidden IN LISTS candidate_target_names)
            list(FIND current_targets "${forbidden}" forbidden_target_position)
            if(NOT forbidden_target_position EQUAL -1)
                message(FATAL_ERROR
                    "VerifyPlaybackCandidateIsolation: ordinary ${inspected} closure links "
                    "candidate target '${forbidden}'")
            endif()
        endforeach()
    endif()
endforeach()

# RR-F302: classify the Chart source table from the actual codemodel. cuexis_chart is always
# created; the codec/tools siblings share the candidate OR developer-tools OR enabled-tests
# creation condition. Each Chart translation unit must belong to exactly one of the three, so a
# source moved back into the base table and a target that compiles a sibling TU are both caught.
set(chart_base_sources
    animation_template_loader.cpp
    canonical_chart_loader.cpp
    canonical_chart_loader_behaviors.cpp
    canonical_chart_loader_primitives.cpp
    canonical_chart_loader_sections.cpp
    canonical_chart_template_internal.cpp
    chart_loader.cpp
    chart_parameter_resolver.cpp
    chart_runtime.cpp
    chart_v4_loader.cpp
    chart_v4_loader_animators.cpp
    chart_v4_loader_parameters.cpp
    chart_v4_loader_projection.cpp
    chart_v4_resolver.cpp
    chart_v4_resolver_patching.cpp
    chart_v4_resolver_program.cpp
    chart_writer.cpp
    prepared_semantic_identity.cpp
    chart_v4_common_reader_internal.cpp
    chart_v4_animation_reader_animator.cpp
    chart_v4_animation_reader_clip.cpp
    chart_v4_animation_reader_values.cpp
    chart_project_path_internal.cpp
    rational_beat.cpp
    timing_map.cpp
    uuid.cpp
)
set(chart_codec_sources
    packed_chart_primitives.cpp
    packed_chart_tables_common.cpp
    packed_chart_tables_decoder.cpp
    packed_chart_tables_decoder_header.cpp
    packed_chart_tables_decoder_identity.cpp
    packed_chart_tables_decoder_model.cpp
    packed_chart_tables_decoder_tables.cpp
    packed_chart_tables_inspect.cpp
    packed_chart_read.cpp
    packed_profile.cpp
    packed_semantic_identity.cpp
)
set(chart_tools_sources
    chart_migrator.cpp
    cxt_v2_loader.cpp
    cxt_v2_loader_expand.cpp
    cxt_v2_loader_parse_nodes.cpp
    cxt_v2_loader_parse_values.cpp
    cxt_v2_loader_reader.cpp
    candidate_lowering.cpp
    candidate_assembler.cpp
    behavior_candidate.cpp
    packed_chart_tables_sections.cpp
    packed_chart_tables_writer.cpp
    packed_chart_io.cpp
)

if(CUEXIS_ENABLE_CHART_V5_CANDIDATE OR CUEXIS_BUILD_DEVELOPER_TOOLS OR CUEXIS_BUILD_TESTS)
    set(chart_support_expected ON)
else()
    set(chart_support_expected OFF)
endif()

file(TO_CMAKE_PATH "${CUEXIS_SOURCE_DIR}" cuexis_source_root)
set(chart_base_include "${cuexis_source_root}/engine/chart/include")
set(chart_codec_include "${cuexis_source_root}/engine/chart/codec_include")
set(chart_tools_include "${cuexis_source_root}/engine/chart/tools_include")

list(FIND target_names cuexis_chart chart_position)
if(chart_position EQUAL -1)
    message(FATAL_ERROR
        "VerifyPlaybackCandidateIsolation: codemodel in '${CUEXIS_BINARY_DIR}' has no "
        "cuexis_chart target")
endif()

# Base Chart: exact source table, base include root only, no codec/tools link edge.
cuexis_assert_cpp_sources(cuexis_chart "${chart_base_sources}")
cuexis_target_include_roots(cuexis_chart chart_include_roots)
cuexis_assert_include_root(cuexis_chart "${chart_base_include}" "${chart_include_roots}")
cuexis_assert_no_include_root(cuexis_chart "${chart_codec_include}" "${chart_include_roots}")
cuexis_assert_no_include_root(cuexis_chart "${chart_tools_include}" "${chart_include_roots}")
cuexis_target_dependency_names(cuexis_chart chart_dependencies)
foreach(forbidden_link cuexis_chart_codec cuexis_chart_tools cuexis_cxc_candidate)
    if(forbidden_link IN_LIST chart_dependencies)
        message(FATAL_ERROR
            "VerifyPlaybackCandidateIsolation: base Chart target 'cuexis_chart' links "
            "'${forbidden_link}'; the base Reader/CXT1/v4 Resolver closure must stay independent")
    endif()
endforeach()

if(chart_support_expected)
    foreach(support_target cuexis_chart_codec cuexis_chart_tools)
        list(FIND target_names "${support_target}" support_position)
        if(support_position EQUAL -1)
            message(FATAL_ERROR
                "VerifyPlaybackCandidateIsolation: support libraries are enabled but the "
                "codemodel has no '${support_target}' target")
        endif()
    endforeach()
    cuexis_assert_cpp_sources(cuexis_chart_codec "${chart_codec_sources}")
    cuexis_assert_cpp_sources(cuexis_chart_tools "${chart_tools_sources}")

    cuexis_target_include_roots(cuexis_chart_codec codec_include_roots)
    cuexis_assert_include_root(cuexis_chart_codec "${chart_codec_include}" "${codec_include_roots}")
    cuexis_assert_no_include_root(cuexis_chart_codec "${chart_tools_include}"
        "${codec_include_roots}")

    cuexis_target_include_roots(cuexis_chart_tools tools_include_roots)
    cuexis_assert_include_root(cuexis_chart_tools "${chart_tools_include}" "${tools_include_roots}")

    # The ordinary Playback and Player closures must not expose the codec/tools build-only roots.
    foreach(inspected IN LISTS inspected_targets)
        if(inspected STREQUAL "cuexis_playback")
            set(current_targets ${playback_targets})
        else()
            set(current_targets ${player_targets})
        endif()
        if(NOT CUEXIS_ENABLE_CHART_V5_CANDIDATE)
            foreach(current_target IN LISTS current_targets)
                cuexis_target_include_roots("${current_target}" closure_include_roots)
                foreach(forbidden_root "${chart_codec_include}" "${chart_tools_include}")
                    if(forbidden_root IN_LIST closure_include_roots)
                        message(FATAL_ERROR
                            "VerifyPlaybackCandidateIsolation: ordinary ${inspected} closure "
                            "target '${current_target}' exposes the Chart support include root "
                            "'${forbidden_root}'")
                    endif()
                endforeach()
            endforeach()
        endif()
    endforeach()
    if(CUEXIS_ENABLE_CHART_V5_CANDIDATE)
        cuexis_target_include_roots(cuexis_playback playback_own_include_roots)
        cuexis_assert_include_root(cuexis_playback "${chart_codec_include}"
            "${playback_own_include_roots}")
        cuexis_assert_include_root(cuexis_playback "${chart_tools_include}"
            "${playback_own_include_roots}")
    endif()
else()
    # The support libraries must not exist when none of the three creation paths is enabled.
    foreach(support_target cuexis_chart_codec cuexis_chart_tools cuexis_cxc_candidate
            cuexis_gameplay_packed)
        list(FIND target_names "${support_target}" support_position)
        if(NOT support_position EQUAL -1)
            message(FATAL_ERROR
                "VerifyPlaybackCandidateIsolation: support library '${support_target}' exists "
                "but the candidate, developer-tools and enabled-tests switches are all OFF")
        endif()
    endforeach()
endif()

# Every Chart translation unit belongs to exactly one library, in every configuration.
set(chart_library_sources
    ${chart_base_sources} ${chart_codec_sources} ${chart_tools_sources})
set(chart_source_owner_pairs "")
foreach(candidate_target IN LISTS target_names)
    # Only Chart libraries can own a Chart translation unit. Other modules may compile a file
    # with the same basename (for example a core uuid.cpp), and that must not count as a second
    # owner of the Chart TU.
    if(NOT candidate_target MATCHES "^cuexis_chart($|_)")
        continue()
    endif()
    cuexis_target_cpp_sources("${candidate_target}" candidate_sources)
    foreach(source IN LISTS candidate_sources)
        if(source IN_LIST chart_library_sources)
            list(APPEND chart_source_owner_pairs "${source}|${candidate_target}")
        endif()
    endforeach()
endforeach()
foreach(source IN LISTS chart_base_sources)
    cuexis_assert_chart_owner("${source}" cuexis_chart "${chart_source_owner_pairs}")
endforeach()
foreach(source IN LISTS chart_codec_sources)
    if(chart_support_expected)
        cuexis_assert_chart_owner("${source}" cuexis_chart_codec "${chart_source_owner_pairs}")
    else()
        foreach(pair IN LISTS chart_source_owner_pairs)
            string(REPLACE "|" ";" parts "${pair}")
            list(GET parts 0 pair_source)
            if(pair_source STREQUAL "${source}")
                list(GET parts 1 pair_target)
                message(FATAL_ERROR
                    "VerifyPlaybackCandidateIsolation: support libraries are disabled but "
                    "target '${pair_target}' compiles codec translation unit '${source}'")
            endif()
        endforeach()
    endif()
endforeach()
foreach(source IN LISTS chart_tools_sources)
    if(chart_support_expected)
        cuexis_assert_chart_owner("${source}" cuexis_chart_tools "${chart_source_owner_pairs}")
    else()
        foreach(pair IN LISTS chart_source_owner_pairs)
            string(REPLACE "|" ";" parts "${pair}")
            list(GET parts 0 pair_source)
            if(pair_source STREQUAL "${source}")
                list(GET parts 1 pair_target)
                message(FATAL_ERROR
                    "VerifyPlaybackCandidateIsolation: support libraries are disabled but "
                    "target '${pair_target}' compiles tools translation unit '${source}'")
            endif()
        endforeach()
    endif()
endforeach()

# The support archives are experimental static-SDK implementation archives. They must not leak
# into the ordinary install script, and a candidate static configuration must actually export
# every archive the experimental closure needs.
set(private_implementation_archives
    cuexis_gameplay_packed
    cuexis_cxc_candidate
    cuexis_chart_codec
    cuexis_chart_tools
)
set(install_script "${CUEXIS_BINARY_DIR}/cmake_install.cmake")
if(EXISTS "${install_script}")
    file(READ "${install_script}" install_text)
    foreach(private_archive IN LISTS private_implementation_archives)
        string(FIND "${install_text}" "${private_archive}" install_private_position)
        if(CUEXIS_ENABLE_CHART_V5_CANDIDATE AND CUEXIS_LIBRARY_TYPE STREQUAL "STATIC")
            if(install_private_position EQUAL -1)
                message(FATAL_ERROR
                    "VerifyPlaybackCandidateIsolation: experimental static install list does "
                    "not export the private implementation archive '${private_archive}'")
            endif()
        elseif(NOT install_private_position EQUAL -1)
            message(FATAL_ERROR
                "VerifyPlaybackCandidateIsolation: this install list exports the private "
                "implementation archive '${private_archive}'")
        endif()
    endforeach()
elseif(NOT CUEXIS_ENABLE_CHART_V5_CANDIDATE)
    message(FATAL_ERROR
        "VerifyPlaybackCandidateIsolation: '${install_script}' is missing; cannot prove the "
        "ordinary install archive list excludes the private implementation archives")
endif()

# Developer tools and enabled tests may link the candidate and Chart support archives for their
# own verification, but the ordinary Playback/Player closure must stay clean. The targets are
# named by the source/dependency checks above whenever they reach an inspected closure.

message(STATUS
    "VerifyPlaybackCandidateIsolation: ${CUEXIS_ENABLE_CHART_V5_CANDIDATE} closure OK "
    "(chart support: ${chart_support_expected}; targets: ${playback_targets}; "
    "sources: ${playback_sources}; player: ${CUEXIS_PLAYER_TARGET})")
