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

    file(GLOB_RECURSE shader_sources
        "${source_dir}/engine/shader/*.cpp"
        "${source_dir}/engine/shader/*.hpp"
    )
    foreach(source IN LISTS shader_sources)
        file(READ "${source}" contents)
        if(contents MATCHES
           "#[ \t]*include[ \t]*[<\"](SDL|glad|GL/|nlohmann/|minizip|cuexis/playback/|cuexis/render_opengl/)")
            message(FATAL_ERROR
                "Shader includes SDL, OpenGL, JSON, archive, Playback or OpenGL adapter headers: ${source}")
        endif()
    endforeach()

    file(GLOB_RECURSE runtime_sources
        "${source_dir}/engine/runtime/*.cpp"
        "${source_dir}/engine/runtime/*.hpp"
    )
    foreach(source IN LISTS runtime_sources)
        file(READ "${source}" contents)
        if(contents MATCHES "#[ \t]*include[ \t]*[<\"](SDL|glad|GL/|cuexis/audio/|cuexis/audio_sdl/|cuexis/platform_sdl/|cuexis/render_opengl/|cuexis/presentation_renderer/|cuexis/shader/|shaderc/|spirv-tools/|spirv_cross/|glslang/)")
            message(FATAL_ERROR "Runtime includes a platform, backend, renderer or shader-compiler header: ${source}")
        endif()
    endforeach()

    file(GLOB_RECURSE playback_sources
        "${source_dir}/engine/playback/*.cpp"
        "${source_dir}/engine/playback/*.hpp"
    )
    foreach(source IN LISTS playback_sources)
        file(READ "${source}" contents)
        if(contents MATCHES
           "#[ \t]*include[ \t]*[<\"](SDL|cuexis/audio_sdl/|cuexis/platform_sdl/|cuexis/render_opengl/|cuexis/presentation_renderer/|cuexis/shader/|shaderc/|spirv-tools/|spirv_cross/|glslang/)")
            message(FATAL_ERROR "Playback includes an adapter, renderer or shader-compiler header: ${source}")
        endif()
    endforeach()

    file(GLOB_RECURSE render_sources
        "${source_dir}/engine/render/*.cpp"
        "${source_dir}/engine/render/*.hpp"
    )
    foreach(source IN LISTS render_sources)
        file(READ "${source}" contents)
        if(contents MATCHES
           "#[ \t]*include[ \t]*[<\"](SDL|glad|GL/|cuexis/playback/|cuexis/presentation_renderer/|cuexis/render_opengl/|cuexis/platform_sdl/)")
            message(FATAL_ERROR "Render includes Playback, the presentation renderer, or a backend header: ${source}")
        endif()
    endforeach()

    file(GLOB_RECURSE player_support_sources
        "${source_dir}/engine/player_support/*.cpp"
        "${source_dir}/engine/player_support/*.hpp"
    )
    foreach(source IN LISTS player_support_sources)
        file(READ "${source}" contents)
        if(contents MATCHES
           "#[ \t]*include[ \t]*[<\"](SDL|glad|GL/|cuexis/playback/|cuexis/render_opengl/|cuexis/audio_sdl/|nlohmann/)")
            message(FATAL_ERROR "Player support includes Playback, SDL, OpenGL, or JSON DOM: ${source}")
        endif()
    endforeach()

    file(GLOB_RECURSE presentation_renderer_sources
        "${source_dir}/engine/presentation_renderer/*.cpp"
        "${source_dir}/engine/presentation_renderer/*.hpp"
    )
    foreach(source IN LISTS presentation_renderer_sources)
        file(READ "${source}" contents)
        if(contents MATCHES
           "#[ \t]*include[ \t]*[<\"](SDL|glad|GL/|cuexis/render_opengl/|cuexis/platform_sdl/|cuexis/audio_sdl/)")
            message(FATAL_ERROR "Presentation renderer includes SDL or OpenGL: ${source}")
        endif()
    endforeach()

    # S7A-1 typed kernel. Ruling S1-04 keeps the input layer inside this module as a separate
    # header partition instead of splitting a cuexis_input target, and keeps the module dependency
    # closure at cuexis::core. A link-level allowlist cannot see a header-only include, so the
    # include list is checked here as well.
    foreach(judgement_header IN ITEMS
            input_boundary.hpp
            role_boundary.hpp
            diagnostic.hpp
            judgement_session.hpp
            timebase.hpp
            gameplay_graph.hpp
            gameplay_assembler.hpp)
        if(NOT EXISTS "${source_dir}/engine/judgement/include/cuexis/judgement/${judgement_header}")
            message(FATAL_ERROR
                "Judgement boundary partition header is missing: ${judgement_header}")
        endif()
    endforeach()

    # S7A-2 keeps the input normalization surface inside the same input-layer partition instead of
    # adding a second input header or a cuexis_input target, so the header that this batch extends is
    # checked for the declarations it now owns. The check is a presence check, not a semantic one:
    # the frozen role and boundary rules themselves are asserted by the judgement tests.
    file(READ
        "${source_dir}/engine/judgement/include/cuexis/judgement/input_boundary.hpp"
        judgement_input_boundary_contents)
    foreach(judgement_input_surface IN ITEMS
            "struct AmountSpec"
            "struct InputDomainDeclaration"
            "struct InputMappingProfile"
            "struct NormalizedObservation"
            "struct RawIngressTimestamps"
            "class SessionIngressState"
            "quantizeAmount"
            "validateInputMapping"
            "normalizeObservation"
            "admitLateQueueEntry")
        if(NOT judgement_input_boundary_contents MATCHES "${judgement_input_surface}")
            message(FATAL_ERROR
                "Judgement S7A-2 input boundary declaration is missing: "
                "${judgement_input_surface}")
        endif()
    endforeach()

    # S7A-3 adds the canonical gameplay graph and the offline typed assembler as two more headers in
    # the same partition rather than a new target or a new module, so the declarations this batch owns
    # are checked for presence here. The check is a presence check, not a semantic one: the frozen
    # rules themselves are asserted by tests/judgement/gameplay_graph_tests.cpp,
    # gameplay_assembler_tests.cpp and gameplay_identity_tests.cpp.
    file(READ
        "${source_dir}/engine/judgement/include/cuexis/judgement/gameplay_graph.hpp"
        judgement_graph_contents)
    foreach(judgement_graph_surface IN ITEMS
            "enum class ReferenceClosure"
            "enum class ReferenceKind"
            "declareReference"
            "struct StableDeclarationId"
            "struct DeclarationRef"
            "struct MergedDeclaration"
            "struct MergedNamespace"
            "struct CapabilityRef"
            "struct FeatureRef"
            "struct RequiredRefs"
            "struct DeclaredCapabilitySet"
            "struct FeatureClosure"
            "struct DerivedCapabilityClosure"
            "struct ResourceRef"
            "struct ResourceClosure"
            "struct ClosureContributions"
            "struct EmissionPathStep"
            "struct RequirementIdentity"
            "class RequiredActionRef"
            "class DomainBindingRef"
            "enum class PatternPrimitive"
            "enum class MatchPolicy"
            "enum class UnsupportedContentKind"
            "struct UnsupportedContentDeclaration"
            "struct PatternNodeDeclaration"
            "struct PatternDeclaration"
            "enum class PhaseKind"
            "enum class FactCategory"
            "enum class Outcome"
            "enum class GradePresence"
            "factCategoryOfPhase"
            "factCategoryToken"
            "struct MeasureComponentDeclaration"
            "struct MeasureSpecDeclaration"
            "enum class ResourceClaimIntent"
            "struct ClaimPolicyDeclaration"
            "enum class GraceOverrideMode"
            "struct GraceOverrideDeclaration"
            "enum class GraceResolutionPolicy"
            "class PreparedGrace"
            "struct GraceDeclaration"
            "struct ResourceClaimDeclaration"
            "struct ResourceRecord"
            "struct JudgementAxisRange"
            "enum class FrameResolution"
            "struct JudgementDomainRecord"
            "validateJudgementDomain"
            "geometryAdd"
            "geometryMultiply"
            "geometrySquare"
            "axisExtent"
            "narrowToAxis"
            "struct RequirementRecord"
            "enum class RelationKind"
            "struct RelationDeclaration"
            "struct SolverProfileDeclaration"
            "struct FactBindingRef"
            "enum class SourceForm"
            "struct SourceClosure"
            "struct DiagnosticMap"
            "struct CanonicalGameplayGraph"
            "canonicalCompare"
            "deriveCapabilityClosure"
            "deriveFeatureClosure"
            "deriveResourceClosure"
            "struct GraphFieldDifference"
            "semanticDiff"
            "equivalent")
        if(NOT judgement_graph_contents MATCHES "${judgement_graph_surface}")
            message(FATAL_ERROR
                "Judgement S7A-3 canonical graph declaration is missing: "
                "${judgement_graph_surface}")
        endif()
    endforeach()

    file(READ
        "${source_dir}/engine/judgement/include/cuexis/judgement/gameplay_assembler.hpp"
        judgement_assembler_contents)
    foreach(judgement_assembler_surface IN ITEMS
            "struct LocalDeclaration"
            "struct GameplaySourceDocument"
            "struct GameplaySource"
            "enum class EntryKind"
            "struct CompileCapabilityContext"
            "struct EngineIdentityDeclaration"
            "struct RulesetIdentityDeclaration"
            "struct SessionIdentityDeclaration"
            "struct PreparedIdentityDeclarations"
            "class CanonicalIdentityBytes"
            "class ChartIdentity"
            "class ContentIdentity"
            "class PreparedIdentity"
            "makeChartIdentity"
            "makeContentIdentity"
            "makePreparedIdentity"
            "sharesJudgementIdentity"
            "struct ContentProfileCounts"
            "struct ContentProfileLimits"
            "enum class ContentProfileVerdict"
            "countContentProfile"
            "checkContentProfile"
            "struct AssemblyRequest"
            "struct AssembledGameplay"
            "assembleGameplay"
            "class GameplayPublication"
            "assembleInto"
            "struct ReferenceClosurePartition"
            "partitionReferences"
            "struct PatternCompileBudget"
            "struct PatternArmBound"
            "checkPatternContainment"
            "struct CompiledMeasureComponent"
            "struct MeasurePhaseContext"
            "class CompiledPattern"
            "class CompiledMeasure"
            "struct GraceResolutionInputs"
            "resolvePreparedGrace"
            "struct ResourceClaimResolutionInputs"
            "class ResourceClaimResolution"
            "compilePattern"
            "compileMeasure"
            "resolveResourceClaims")
        if(NOT judgement_assembler_contents MATCHES "${judgement_assembler_surface}")
            message(FATAL_ERROR
                "Judgement S7A-3 offline assembler declaration is missing: "
                "${judgement_assembler_surface}")
        endif()
    endforeach()

    file(GLOB_RECURSE judgement_sources
        "${source_dir}/engine/judgement/*.cpp"
        "${source_dir}/engine/judgement/*.hpp"
    )
    foreach(source IN LISTS judgement_sources)
        file(READ "${source}" contents)
        string(REGEX MATCHALL "#[ \t]*include[ \t]*[<\"]cuexis/[^>\"]*[>\"]"
               judgement_includes "${contents}")
        foreach(judgement_include IN LISTS judgement_includes)
            if(NOT judgement_include MATCHES "cuexis/(core|judgement)/")
                message(FATAL_ERROR
                    "Judgement includes a non-core Cuexis header: ${source} -> ${judgement_include}")
            endif()
        endforeach()
        if(contents MATCHES
           "#[ \t]*include[ \t]*[<\"](SDL|glad|GL/|glm/|entt/|nlohmann/|minizip|spdlog/|shaderc/|spirv-tools/|spirv_cross/|glslang/)")
            message(FATAL_ERROR
                "Judgement includes a platform, backend or third-party implementation header: "
                "${source}")
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
        if(contents MATCHES "#[ \t]*include[ \t]*[<\"](entt/|SDL|glad/|GL/|nlohmann/|spdlog/|shaderc/|spirv-tools/|spirv_cross/|glslang/|cuexis/shader/)")
            message(FATAL_ERROR
                "PlaybackSession public header leaked backend or implementation header: ${source}")
        endif()
        if(contents MATCHES "cuexis::runtime::RuntimeSession|cuexis::world::World[^T]")
            message(FATAL_ERROR
                "PlaybackSession public header leaked internal Runtime/World type: ${source}")
        endif()
    endforeach()

    set(player_neutral_sources
        "${source_dir}/app/player/src/player_options.hpp"
        "${source_dir}/app/player/src/player_options.cpp"
        "${source_dir}/app/player/src/player_surface.hpp"
        "${source_dir}/app/player/src/player_audio_seat.hpp"
        "${source_dir}/app/player/src/player_control.hpp"
        "${source_dir}/app/player/src/player_control.cpp"
    )
    foreach(source IN LISTS player_neutral_sources)
        if(NOT EXISTS "${source}")
            message(FATAL_ERROR "Player neutral source is missing: ${source}")
        endif()
        file(READ "${source}" contents)
        if(contents MATCHES
           "#[ \t]*include[ \t]*[<\"](SDL|glad|GL/|cuexis/audio_sdl/|cuexis/platform_sdl/|cuexis/render_opengl/)")
            message(FATAL_ERROR "Player control or options include an adapter header: ${source}")
        endif()
    endforeach()

    file(READ "${source_dir}/CMakeLists.txt" root_lists)
    foreach(install_list IN ITEMS CUEXIS_PUBLIC_EXPORT_TARGETS CUEXIS_STATIC_IMPLEMENTATION_TARGETS)
        string(REGEX MATCH "set\\(${install_list}[^)]*\\)" install_block "${root_lists}")
        if(NOT install_block)
            message(FATAL_ERROR "Missing install target list ${install_list}")
        endif()
        if(install_block MATCHES "cuexis_presentation_renderer|cuexis_player_support")
            message(FATAL_ERROR
                "An internal Stage 6 target must not be installed: ${install_list}")
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
