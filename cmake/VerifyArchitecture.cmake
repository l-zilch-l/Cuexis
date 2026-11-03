# The declaration checks below read header text without running a preprocessor. Strip comments and
# the branches a preprocessor would not compile, so a declaration has to be live code: a commented-out
# definition, one inside `#if 0`, or one in the dead half of `#if 0 ... #else ... #endif` must not
# satisfy the gate. `#if`/`#ifdef`/`#ifndef` other than `#if 0` are treated as live.
function(_cuexis_strip_inactive text output_variable)
    string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" " " stripped "${text}")
    string(REGEX REPLACE "//[^\n]*" " " stripped "${stripped}")
    # A semicolon cannot live inside one CMake list element, so park it while the text is split into
    # lines and put it back before returning.
    string(REPLACE ";" "@cuexis_semicolon@" stripped "${stripped}")
    string(REPLACE "\n" ";" lines "${stripped}")
    set(live_stack "")
    set(kept "")
    foreach(line IN LISTS lines)
        if(line MATCHES "^[ \t]*#[ \t]*if[ \t]+0[ \t]*$")
            list(APPEND live_stack 0)
        elseif(line MATCHES "^[ \t]*#[ \t]*if[ \t]+1[ \t]*$")
            list(APPEND live_stack 1)
        elseif(line MATCHES "^[ \t]*#[ \t]*if(n?def)?[ \t]")
            # A condition this script cannot evaluate is assumed live, which keeps the check from
            # failing on a declaration that a real build would see.
            list(APPEND live_stack 2)
        elseif(line MATCHES "^[ \t]*#[ \t]*(else|elif)")
            list(LENGTH live_stack depth)
            if(depth GREATER 0)
                math(EXPR top "${depth} - 1")
                list(GET live_stack ${top} branch)
                if(branch EQUAL 0)
                    list(REMOVE_AT live_stack ${top})
                    list(APPEND live_stack 1)
                elseif(branch EQUAL 1)
                    list(REMOVE_AT live_stack ${top})
                    list(APPEND live_stack 0)
                endif()
            endif()
        elseif(line MATCHES "^[ \t]*#[ \t]*endif")
            list(LENGTH live_stack depth)
            if(depth GREATER 0)
                math(EXPR top "${depth} - 1")
                list(REMOVE_AT live_stack ${top})
            endif()
        endif()
        list(FIND live_stack 0 dead)
        if(dead EQUAL -1)
            string(APPEND kept "${line}\n")
        endif()
    endforeach()
    string(REPLACE "@cuexis_semicolon@" ";" kept "${kept}")
    set(${output_variable} "${kept}" PARENT_SCOPE)
endfunction()

# A declaration entry is "owning-file|surface". Both halves are needed: the surface names what must
# exist, the owner names the partition header that has to declare it.
function(_cuexis_split_entry entry owner_variable surface_variable)
    string(REPLACE "|" ";" fields "${entry}")
    list(GET fields 0 owner)
    list(GET fields 1 surface)
    set(${owner_variable} "${owner}" PARENT_SCOPE)
    set(${surface_variable} "${surface}" PARENT_SCOPE)
endfunction()

# Stripped text of one partition header, read and processed once per gate run.
function(_cuexis_partition_text source_dir directory file output_variable)
    string(REPLACE "/" "_" cache_key "${directory}/${file}")
    string(REPLACE "." "_" cache_key "${cache_key}")
    get_property(cached GLOBAL PROPERTY "CUEXIS_PARTITION_${cache_key}")
    if(cached)
        set(${output_variable} "${cached}" PARENT_SCOPE)
        return()
    endif()
    file(READ "${source_dir}/${directory}/${file}" contents)
    _cuexis_strip_inactive("${contents}" stripped)
    set_property(GLOBAL PROPERTY "CUEXIS_PARTITION_${cache_key}" "${stripped}")
    set(${output_variable} "${stripped}" PARENT_SCOPE)
endfunction()

# Build the pattern that a surface entry must match. A type entry has to be a definition, not a
# forward declaration: the optional group after the name starts with whitespace or ':' so that a
# longer name (`AmountSpecification`) cannot satisfy `AmountSpec`, while `struct X final {`,
# `struct X{` and `enum class X : std::uint8_t {` all still match. A function entry has to look like
# a declaration or definition: the name followed by its parameter list.
function(_cuexis_declaration_pattern surface output_variable)
    if(surface MATCHES "^(struct|class|enum class) ")
        set(${output_variable} "${surface}([ \t\r\n:][^;{}]*)?[{]" PARENT_SCOPE)
    else()
        set(${output_variable} "${surface}[ \t\r\n]*\\(" PARENT_SCOPE)
    endif()
endfunction()

# A split public header is an umbrella that includes its parts. The declaration checks read the
# parts directly, so a stale umbrella would still pass while consumers of the umbrella lost the
# declarations; require every part to be included by the umbrella or by another part.
function(_cuexis_require_partition_includes source_dir directory umbrella)
    set(partition_files ${umbrella} ${ARGN})
    set(include_text "")
    foreach(partition_file IN LISTS partition_files)
        file(READ "${source_dir}/${directory}/${partition_file}" partition_contents)
        string(APPEND include_text "${partition_contents}\n")
    endforeach()
    foreach(part IN LISTS ARGN)
        if(NOT include_text MATCHES "#[ \t]*include[ \t]*[<\"]${part}[>\"]")
            message(FATAL_ERROR
                "Partition header ${part} is not included by ${umbrella} or by another part of the "
                "same partition, so the umbrella no longer exposes it")
        endif()
    endforeach()
endfunction()

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
    # the frozen role and boundary rules themselves are asserted by the judgement tests. Every entry
    # names the partition header that owns it, so a declaration that drifts into a sibling file fails
    # here instead of hiding in the concatenation of the whole partition.
    _cuexis_require_partition_includes("${source_dir}"
        "engine/judgement/include/cuexis/judgement"
        input_boundary.hpp
        input_boundary_amount.hpp
        input_boundary_mapping.hpp)
    foreach(judgement_input_entry IN ITEMS
            "input_boundary_amount.hpp|struct AmountSpec"
            "input_boundary_amount.hpp|struct InputDomainDeclaration"
            "input_boundary_mapping.hpp|struct InputMappingProfile"
            "input_boundary.hpp|struct NormalizedObservation"
            "input_boundary.hpp|struct RawIngressTimestamps"
            "input_boundary.hpp|class SessionIngressState"
            "input_boundary_amount.hpp|quantizeAmount"
            "input_boundary_mapping.hpp|validateInputMapping"
            "input_boundary.hpp|normalizeObservation"
            "input_boundary.hpp|admitLateQueueEntry")
        _cuexis_split_entry("${judgement_input_entry}" judgement_input_owner judgement_input_surface)
        _cuexis_partition_text("${source_dir}" "engine/judgement/include/cuexis/judgement"
            "${judgement_input_owner}" judgement_input_text)
        _cuexis_declaration_pattern("${judgement_input_surface}" judgement_input_pattern)
        if(NOT judgement_input_text MATCHES "${judgement_input_pattern}")
            message(FATAL_ERROR
                "Judgement S7A-2 input boundary declaration is missing: "
                "${judgement_input_surface} (owner ${judgement_input_owner})")
        endif()
    endforeach()

    # S7A-3 adds the canonical gameplay graph and the offline typed assembler as two more headers in
    # the same partition rather than a new target or a new module, so the declarations this batch owns
    # are checked for presence here. The check is a presence check, not a semantic one: the frozen
    # rules themselves are asserted by tests/judgement/gameplay_graph_tests.cpp,
    # gameplay_assembler_tests.cpp and gameplay_identity_tests.cpp.
    _cuexis_require_partition_includes("${source_dir}"
        "engine/judgement/include/cuexis/judgement"
        gameplay_graph.hpp
        gameplay_graph_reference.hpp
        gameplay_graph_pattern.hpp
        gameplay_graph_phase.hpp
        gameplay_graph_record.hpp)
    foreach(judgement_graph_entry IN ITEMS
            "gameplay_graph_reference.hpp|enum class ReferenceClosure"
            "gameplay_graph_reference.hpp|enum class ReferenceKind"
            "gameplay_graph_reference.hpp|declareReference"
            "gameplay_graph_reference.hpp|struct StableDeclarationId"
            "gameplay_graph_reference.hpp|struct DeclarationRef"
            "gameplay_graph_reference.hpp|struct MergedDeclaration"
            "gameplay_graph_reference.hpp|struct MergedNamespace"
            "gameplay_graph_reference.hpp|struct CapabilityRef"
            "gameplay_graph_reference.hpp|struct FeatureRef"
            "gameplay_graph_reference.hpp|struct RequiredRefs"
            "gameplay_graph_reference.hpp|struct DeclaredCapabilitySet"
            "gameplay_graph_reference.hpp|struct FeatureClosure"
            "gameplay_graph_reference.hpp|struct DerivedCapabilityClosure"
            "gameplay_graph_reference.hpp|struct ResourceRef"
            "gameplay_graph_reference.hpp|struct ResourceClosure"
            "gameplay_graph_reference.hpp|struct ClosureContributions"
            "gameplay_graph_pattern.hpp|struct EmissionPathStep"
            "gameplay_graph_pattern.hpp|struct RequirementIdentity"
            "gameplay_graph_pattern.hpp|class RequiredActionRef"
            "gameplay_graph_pattern.hpp|class DomainBindingRef"
            "gameplay_graph_pattern.hpp|enum class PatternPrimitive"
            "gameplay_graph_pattern.hpp|enum class MatchPolicy"
            "gameplay_graph_pattern.hpp|enum class UnsupportedContentKind"
            "gameplay_graph_pattern.hpp|struct UnsupportedContentDeclaration"
            "gameplay_graph_pattern.hpp|struct PatternNodeDeclaration"
            "gameplay_graph_pattern.hpp|struct PatternDeclaration"
            "gameplay_graph_phase.hpp|enum class PhaseKind"
            "gameplay_graph_phase.hpp|enum class FactCategory"
            "gameplay_graph_phase.hpp|enum class Outcome"
            "gameplay_graph_phase.hpp|enum class GradePresence"
            "gameplay_graph_phase.hpp|factCategoryOfPhase"
            "gameplay_graph_phase.hpp|factCategoryToken"
            "gameplay_graph_phase.hpp|struct MeasureComponentDeclaration"
            "gameplay_graph_phase.hpp|struct MeasureSpecDeclaration"
            "gameplay_graph_phase.hpp|enum class ResourceClaimIntent"
            "gameplay_graph_phase.hpp|struct ClaimPolicyDeclaration"
            "gameplay_graph_phase.hpp|enum class GraceOverrideMode"
            "gameplay_graph_phase.hpp|struct GraceOverrideDeclaration"
            "gameplay_graph_phase.hpp|enum class GraceResolutionPolicy"
            "gameplay_graph_phase.hpp|class PreparedGrace"
            "gameplay_graph_phase.hpp|struct GraceDeclaration"
            "gameplay_graph_phase.hpp|struct ResourceClaimDeclaration"
            "gameplay_graph_phase.hpp|struct ResourceRecord"
            "gameplay_graph_record.hpp|struct JudgementAxisRange"
            "gameplay_graph_record.hpp|enum class FrameResolution"
            "gameplay_graph_record.hpp|struct JudgementDomainRecord"
            "gameplay_graph_record.hpp|validateJudgementDomain"
            "gameplay_graph_record.hpp|geometryAdd"
            "gameplay_graph_record.hpp|geometryMultiply"
            "gameplay_graph_record.hpp|geometrySquare"
            "gameplay_graph_record.hpp|axisExtent"
            "gameplay_graph_record.hpp|narrowToAxis"
            "gameplay_graph_record.hpp|struct RequirementRecord"
            "gameplay_graph_record.hpp|enum class RelationKind"
            "gameplay_graph_record.hpp|struct RelationDeclaration"
            "gameplay_graph_record.hpp|struct SolverProfileDeclaration"
            "gameplay_graph_record.hpp|struct FactBindingRef"
            "gameplay_graph_record.hpp|enum class SourceForm"
            "gameplay_graph_record.hpp|struct SourceClosure"
            "gameplay_graph_record.hpp|struct DiagnosticMap"
            "gameplay_graph.hpp|struct CanonicalGameplayGraph"
            "gameplay_graph.hpp|canonicalCompare"
            "gameplay_graph.hpp|deriveCapabilityClosure"
            "gameplay_graph.hpp|deriveFeatureClosure"
            "gameplay_graph.hpp|deriveResourceClosure"
            "gameplay_graph.hpp|struct GraphFieldDifference"
            "gameplay_graph.hpp|semanticDiff"
            "gameplay_graph.hpp|equivalent")
        _cuexis_split_entry("${judgement_graph_entry}" judgement_graph_owner judgement_graph_surface)
        _cuexis_partition_text("${source_dir}" "engine/judgement/include/cuexis/judgement"
            "${judgement_graph_owner}" judgement_graph_text)
        _cuexis_declaration_pattern("${judgement_graph_surface}" judgement_graph_pattern)
        if(NOT judgement_graph_text MATCHES "${judgement_graph_pattern}")
            message(FATAL_ERROR
                "Judgement S7A-3 canonical graph declaration is missing: "
                "${judgement_graph_surface} (owner ${judgement_graph_owner})")
        endif()
    endforeach()

    _cuexis_require_partition_includes("${source_dir}"
        "engine/judgement/include/cuexis/judgement"
        gameplay_assembler.hpp
        gameplay_assembler_identity.hpp
        gameplay_assembler_publication.hpp
        gameplay_assembler_compiled.hpp)
    foreach(judgement_assembler_entry IN ITEMS
            "gameplay_assembler_identity.hpp|struct LocalDeclaration"
            "gameplay_assembler_identity.hpp|struct GameplaySourceDocument"
            "gameplay_assembler_identity.hpp|struct GameplaySource"
            "gameplay_assembler_identity.hpp|enum class EntryKind"
            "gameplay_assembler_identity.hpp|struct CompileCapabilityContext"
            "gameplay_assembler_identity.hpp|struct EngineIdentityDeclaration"
            "gameplay_assembler_identity.hpp|struct RulesetIdentityDeclaration"
            "gameplay_assembler_identity.hpp|struct SessionIdentityDeclaration"
            "gameplay_assembler_identity.hpp|struct PreparedIdentityDeclarations"
            "gameplay_assembler_identity.hpp|class CanonicalIdentityBytes"
            "gameplay_assembler_identity.hpp|class ChartIdentity"
            "gameplay_assembler_identity.hpp|class ContentIdentity"
            "gameplay_assembler_identity.hpp|class PreparedIdentity"
            "gameplay_assembler_identity.hpp|makeChartIdentity"
            "gameplay_assembler_identity.hpp|makeContentIdentity"
            "gameplay_assembler_identity.hpp|makePreparedIdentity"
            "gameplay_assembler_identity.hpp|sharesJudgementIdentity"
            "gameplay_assembler_publication.hpp|struct ContentProfileCounts"
            "gameplay_assembler_publication.hpp|struct ContentProfileLimits"
            "gameplay_assembler_publication.hpp|enum class ContentProfileVerdict"
            "gameplay_assembler_publication.hpp|countContentProfile"
            "gameplay_assembler_publication.hpp|checkContentProfile"
            "gameplay_assembler_publication.hpp|struct AssemblyRequest"
            "gameplay_assembler_publication.hpp|struct AssembledGameplay"
            "gameplay_assembler_publication.hpp|assembleGameplay"
            "gameplay_assembler_publication.hpp|class GameplayPublication"
            "gameplay_assembler_publication.hpp|assembleInto"
            "gameplay_assembler_publication.hpp|struct ReferenceClosurePartition"
            "gameplay_assembler_publication.hpp|partitionReferences"
            "gameplay_assembler_compiled.hpp|struct PatternCompileBudget"
            "gameplay_assembler_compiled.hpp|struct PatternArmBound"
            "gameplay_assembler_compiled.hpp|checkPatternContainment"
            "gameplay_assembler_compiled.hpp|struct CompiledMeasureComponent"
            "gameplay_assembler_compiled.hpp|struct MeasurePhaseContext"
            "gameplay_assembler_compiled.hpp|class CompiledPattern"
            "gameplay_assembler_compiled.hpp|class CompiledMeasure"
            "gameplay_assembler.hpp|struct GraceResolutionInputs"
            "gameplay_assembler.hpp|resolvePreparedGrace"
            "gameplay_assembler.hpp|struct ResourceClaimResolutionInputs"
            "gameplay_assembler.hpp|class ResourceClaimResolution"
            "gameplay_assembler_compiled.hpp|compilePattern"
            "gameplay_assembler_compiled.hpp|compileMeasure"
            "gameplay_assembler.hpp|resolveResourceClaims")
        _cuexis_split_entry("${judgement_assembler_entry}" judgement_assembler_owner judgement_assembler_surface)
        _cuexis_partition_text("${source_dir}" "engine/judgement/include/cuexis/judgement"
            "${judgement_assembler_owner}" judgement_assembler_text)
        _cuexis_declaration_pattern("${judgement_assembler_surface}" judgement_assembler_pattern)
        if(NOT judgement_assembler_text MATCHES "${judgement_assembler_pattern}")
            message(FATAL_ERROR
                "Judgement S7A-3 offline assembler declaration is missing: "
                "${judgement_assembler_surface} (owner ${judgement_assembler_owner})")
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
