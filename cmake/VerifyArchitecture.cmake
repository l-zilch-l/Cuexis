function(cuexis_verify_source_architecture source_dir)
    file(GLOB_RECURSE core_sources
        "${source_dir}/engine/core/*.cpp"
        "${source_dir}/engine/core/*.hpp"
    )

    foreach(source IN LISTS core_sources)
        file(READ "${source}" contents)
        if(contents MATCHES "#[ \t]*include[ \t]*[<\"](SDL|glad|GL/)")
            message(FATAL_ERROR "Core includes a platform or graphics header: ${source}")
        endif()
    endforeach()

    file(GLOB_RECURSE chart_sources
        "${source_dir}/engine/chart/*.cpp"
        "${source_dir}/engine/chart/*.hpp"
    )
    foreach(source IN LISTS chart_sources)
        file(READ "${source}" contents)
        if(contents MATCHES "#[ \t]*include[ \t]*[<\"](entt/|cuexis/world/|cuexis/audio/|cuexis/audio_sdl/|SDL|glad|GL/)")
            message(FATAL_ERROR "Chart includes a World, platform or graphics header: ${source}")
        endif()
    endforeach()

    file(GLOB_RECURSE cxc_sources
        "${source_dir}/engine/cxc/*.cpp"
        "${source_dir}/engine/cxc/*.hpp"
    )
    foreach(source IN LISTS cxc_sources)
        file(READ "${source}" contents)
        if(contents MATCHES
           "#[ \t]*include[ \t]*[<\"](entt/|cuexis/playback/|cuexis/runtime/|cuexis/world/|cuexis/audio/|cuexis/audio_sdl/|cuexis/platform_sdl/|cuexis/render/|cuexis/render_opengl/|SDL|glad|GL/)")
            message(FATAL_ERROR "CXC includes a runtime, world, audio, platform or render header: ${source}")
        endif()
    endforeach()

    file(GLOB_RECURSE audio_sources
        "${source_dir}/engine/audio/*.cpp"
        "${source_dir}/engine/audio/*.hpp"
    )
    foreach(source IN LISTS audio_sources)
        file(READ "${source}" contents)
        if(contents MATCHES
           "#[ \t]*include[ \t]*[<\"](SDL|cuexis/audio_sdl/|cuexis/platform_sdl/)")
            message(FATAL_ERROR "Audio core includes an SDL adapter header: ${source}")
        endif()
    endforeach()

    file(GLOB_RECURSE audio_sdl_sources
        "${source_dir}/engine/audio_sdl/*.cpp"
        "${source_dir}/engine/audio_sdl/*.hpp"
    )
    foreach(source IN LISTS audio_sdl_sources)
        file(READ "${source}" contents)
        if(contents MATCHES "#[ \t]*include[ \t]*[<\"]cuexis/platform_sdl/")
            message(FATAL_ERROR "AudioSDL includes the platform SDL adapter: ${source}")
        endif()
    endforeach()

    file(GLOB_RECURSE animation_sources
        "${source_dir}/engine/animation/*.cpp"
        "${source_dir}/engine/animation/*.hpp"
    )
    foreach(source IN LISTS animation_sources)
        file(READ "${source}" contents)
        if(contents MATCHES
           "#[ \t]*include[ \t]*[<\"](nlohmann/|minizip|SDL|glad|GL/|cuexis/cxc/|cuexis/playback/|cuexis/audio_sdl/|cuexis/platform_sdl/|cuexis/render_opengl/|cuexis/json_support/|cuexis/chart/animation_template_document.hpp|cuexis/chart/chart_v4_loader.hpp|cuexis/chart/chart_v4_resolver.hpp)")
            message(FATAL_ERROR
                "Animation includes JSON, CXC, CXT source, Playback, adapter or resolver headers: ${source}")
        endif()
    endforeach()

    file(GLOB_RECURSE runtime_sources
        "${source_dir}/engine/runtime/*.cpp"
        "${source_dir}/engine/runtime/*.hpp"
    )
    foreach(source IN LISTS runtime_sources)
        file(READ "${source}" contents)
        if(contents MATCHES "#[ \t]*include[ \t]*[<\"](SDL|glad|GL/|cuexis/audio/|cuexis/audio_sdl/|cuexis/platform_sdl/|cuexis/render_opengl/)")
            message(FATAL_ERROR "Runtime includes a platform or backend header: ${source}")
        endif()
    endforeach()

    file(GLOB_RECURSE playback_sources
        "${source_dir}/engine/playback/*.cpp"
        "${source_dir}/engine/playback/*.hpp"
    )
    foreach(source IN LISTS playback_sources)
        file(READ "${source}" contents)
        if(contents MATCHES
           "#[ \t]*include[ \t]*[<\"](SDL|cuexis/audio_sdl/|cuexis/platform_sdl/|cuexis/render_opengl/)")
            message(FATAL_ERROR "Playback includes an adapter header: ${source}")
        endif()
    endforeach()

    file(GLOB_RECURSE all_project_sources
        "${source_dir}/app/*.cpp"
        "${source_dir}/app/*.hpp"
        "${source_dir}/engine/*.cpp"
        "${source_dir}/engine/*.hpp"
        "${source_dir}/tests/*.cpp"
        "${source_dir}/tests/*.hpp"
    )

    foreach(source IN LISTS all_project_sources)
        file(READ "${source}" contents)
        if(NOT source MATCHES "[/\\\\]engine[/\\\\]json_support[/\\\\]" AND
           (contents MATCHES "#[ \t]*include[ \t]*[<\"]nlohmann/" OR
            contents MATCHES "(^|[^A-Za-z0-9_])nlohmann::"))
            message(FATAL_ERROR "nlohmann JSON types escaped json_support: ${source}")
        endif()

        if(source MATCHES "[/\\\\]engine[/\\\\]render_opengl[/\\\\]")
            continue()
        endif()
        if(contents MATCHES "#[ \t]*include[ \t]*[<\"](glad|GL/)")
            message(FATAL_ERROR "OpenGL header used outside render_opengl: ${source}")
        endif()
        if(contents MATCHES "(^|[^A-Za-z0-9_])gl[A-Z][A-Za-z0-9_]*[ \t\r\n]*\\(")
            message(FATAL_ERROR "OpenGL call used outside render_opengl: ${source}")
        endif()
    endforeach()

    file(GLOB_RECURSE public_headers "${source_dir}/engine/*/include/*.hpp")
    foreach(source IN LISTS public_headers)
        file(READ "${source}" contents)
        if(contents MATCHES "[^ -~\t\r\n]")
            message(FATAL_ERROR "Non-ASCII text escaped into an installed public header: ${source}")
        endif()
        if(contents MATCHES "#[ \t]*include[ \t]*[<\"]glm/" OR
           contents MATCHES "(^|[^A-Za-z0-9_])glm::")
            message(FATAL_ERROR "GLM type escaped a Cuexis public header: ${source}")
        endif()
    endforeach()

    # SDK public header check — PlaybackSession must not expose EnTT, SDL, OpenGL, or World types.
    file(GLOB_RECURSE playback_public_headers
        "${source_dir}/engine/playback/include/*.hpp")
    foreach(source IN LISTS playback_public_headers)
        file(READ "${source}" contents)
        if(contents MATCHES "#[ \t]*include[ \t]*[<\"](entt/|SDL|glad/|GL/|nlohmann/|spdlog/)")
            message(FATAL_ERROR
                "PlaybackSession public header leaked backend or implementation header: ${source}")
        endif()
        if(contents MATCHES "cuexis::runtime::RuntimeSession|cuexis::world::World[^T]")
            message(FATAL_ERROR
                "PlaybackSession public header leaked internal Runtime/World type: ${source}")
        endif()
    endforeach()
endfunction()

function(_cuexis_collect_buildsystem_targets directory output_variable)
    get_property(local_targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)

    set(all_targets ${local_targets})
    foreach(subdirectory IN LISTS subdirectories)
        _cuexis_collect_buildsystem_targets("${subdirectory}" child_targets)
        list(APPEND all_targets ${child_targets})
    endforeach()

    set(${output_variable} ${all_targets} PARENT_SCOPE)
endfunction()

function(cuexis_verify_active_targets)
    cmake_parse_arguments(PARSE_ARGV 0 argument "" "SOURCE_DIR" "ALLOWED")
    if(argument_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "cuexis_verify_active_targets received unexpected arguments: "
            "${argument_UNPARSED_ARGUMENTS}"
        )
    endif()
    if(NOT argument_SOURCE_DIR)
        message(FATAL_ERROR "cuexis_verify_active_targets requires SOURCE_DIR")
    endif()

    _cuexis_collect_buildsystem_targets("${argument_SOURCE_DIR}" project_targets)
    list(FILTER project_targets INCLUDE REGEX "^cuexis_")
    list(REMOVE_DUPLICATES project_targets)
    list(SORT project_targets)

    set(allowed_targets ${argument_ALLOWED})
    list(REMOVE_DUPLICATES allowed_targets)
    list(SORT allowed_targets)

    if(NOT "${project_targets}" STREQUAL "${allowed_targets}")
        message(FATAL_ERROR
            "Active Cuexis target set differs from the current-stage allowlist.\n"
            "  Actual: ${project_targets}\n"
            "  Allowed: ${allowed_targets}"
        )
    endif()
endfunction()

function(_cuexis_normalize_link_item item output_variable)
    if(item MATCHES "^\\$<LINK_ONLY:(.*)>$")
        set(item "${CMAKE_MATCH_1}")
    endif()

    # CMake may wrap cross-directory link entries in directory-id sentinels.
    if(item MATCHES "^::@")
        set(item "")
    endif()

    set(${output_variable} "${item}" PARENT_SCOPE)
endfunction()

function(cuexis_verify_target_dependencies target)
    cmake_parse_arguments(PARSE_ARGV 1 argument "" "" "ALLOWED")
    if(argument_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "cuexis_verify_target_dependencies received unexpected arguments for ${target}: "
            "${argument_UNPARSED_ARGUMENTS}"
        )
    endif()
    if(NOT TARGET ${target})
        message(FATAL_ERROR "Cannot verify missing Cuexis target: ${target}")
    endif()

    set(actual_dependencies)
    foreach(property LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
        get_target_property(property_value ${target} ${property})
        if(NOT property_value OR property_value MATCHES "-NOTFOUND$")
            continue()
        endif()

        foreach(item IN LISTS property_value)
            _cuexis_normalize_link_item("${item}" normalized_item)
            if(normalized_item)
                list(APPEND actual_dependencies "${normalized_item}")
            endif()
        endforeach()
    endforeach()
    list(REMOVE_DUPLICATES actual_dependencies)

    foreach(dependency IN LISTS actual_dependencies)
        if(NOT dependency IN_LIST argument_ALLOWED)
            message(FATAL_ERROR
                "Cuexis target ${target} directly links non-allowlisted dependency "
                "${dependency}. Allowed dependencies: ${argument_ALLOWED}"
            )
        endif()
    endforeach()
endfunction()

if(CMAKE_SCRIPT_MODE_FILE)
    if(NOT DEFINED CUEXIS_SOURCE_DIR)
        message(FATAL_ERROR "CUEXIS_SOURCE_DIR is required")
    endif()
    cuexis_verify_source_architecture("${CUEXIS_SOURCE_DIR}")
endif()
