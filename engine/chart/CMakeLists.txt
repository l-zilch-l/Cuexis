add_library(cuexis_chart STATIC)
add_library(cuexis::chart ALIAS cuexis_chart)

set_target_properties(cuexis_chart PROPERTIES EXPORT_NAME InternalChart)

# The base Chart library keeps the old Reader, CXT1, the v4 loader/resolver/runtime and
# chart_writer.cpp. Packed codec and source/tooling code live in the two optional siblings so
# the ordinary Playback and Player closures cannot reach them. Each translation unit belongs to
# exactly one target; the shared private headers under src/ are included, never recompiled.
target_sources(cuexis_chart
    PRIVATE
        src/animation_template_loader.cpp
        src/canonical_chart_loader.cpp
        src/canonical_chart_loader_behaviors.cpp
        src/canonical_chart_loader_primitives.cpp
        src/canonical_chart_loader_sections.cpp
        src/canonical_chart_loader_units_internal.hpp
        src/canonical_chart_template_internal.cpp
        src/chart_loader.cpp
        src/chart_parameter_resolver.cpp
        src/chart_runtime.cpp
        src/chart_v4_loader.cpp
        src/chart_v4_loader_animators.cpp
        src/chart_v4_loader_parameters.cpp
        src/chart_v4_loader_projection.cpp
        src/chart_v4_resolver.cpp
        src/chart_v4_resolver_patching.cpp
        src/chart_v4_resolver_program.cpp
        src/chart_v4_resolver_units_internal.hpp
        src/chart_writer.cpp
        src/prepared_semantic_identity.cpp
        src/chart_v4_common_reader_internal.cpp
        src/chart_v4_animation_reader_animator.cpp
        src/chart_v4_animation_reader_clip.cpp
        src/chart_v4_animation_reader_values.cpp
        src/chart_project_path_internal.cpp
        src/rational_beat.cpp
        src/timing_map.cpp
        src/timing_source_adapter.cpp
        src/uuid.cpp
    PRIVATE
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/cuexis/chart/animation_program_input.hpp
            include/cuexis/chart/animation_template_document.hpp
            include/cuexis/chart/animation_template_loader.hpp
            include/cuexis/chart/canonical_chart_loader.hpp
            include/cuexis/chart/chart_document.hpp
            include/cuexis/chart/chart_loader.hpp
            include/cuexis/chart/chart_runtime.hpp
            include/cuexis/chart/chart_v4_document.hpp
            include/cuexis/chart/chart_v4_loader.hpp
            include/cuexis/chart/chart_v4_resolver.hpp
            include/cuexis/chart/chart_writer.hpp
            include/cuexis/chart/limits.hpp
            include/cuexis/chart/prepared_semantic_identity.hpp
            include/cuexis/chart/rational_beat.hpp
            include/cuexis/chart/timing_map.hpp
            include/cuexis/chart/timing_source_adapter.hpp
            include/cuexis/chart/uuid.hpp
)

target_include_directories(cuexis_chart PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(cuexis_chart PUBLIC cxx_std_20)

target_link_libraries(cuexis_chart
    PUBLIC
        cuexis::core
        cuexis::timing
    PRIVATE
        cuexis::json_support
)

# The two optional libraries share one creation condition. Tests and developer tools may build
# them as support libraries, but the ordinary Playback/Player link closure never consumes them.
if(CUEXIS_ENABLE_CHART_V5_CANDIDATE OR CUEXIS_BUILD_DEVELOPER_TOOLS OR
   (BUILD_TESTING AND CUEXIS_BUILD_TESTS))
    add_library(cuexis_chart_codec STATIC)
    add_library(cuexis::chart_codec ALIAS cuexis_chart_codec)
    set_target_properties(cuexis_chart_codec PROPERTIES EXPORT_NAME InternalChartCodec)

    # Pure Packed Reader and primitive/identity codec. No CXT2 expansion, source lowering,
    # Behavior compilation, migration or file Writer may be compiled here.
    target_sources(cuexis_chart_codec
        PRIVATE
            src/packed_chart_primitives.cpp
            src/packed_chart_tables_common.cpp
            src/packed_chart_tables_decoder.cpp
            src/packed_chart_tables_decoder_header.cpp
            src/packed_chart_tables_decoder_identity.cpp
            src/packed_chart_tables_decoder_model.cpp
            src/packed_chart_tables_decoder_tables.cpp
            src/packed_chart_tables_inspect.cpp
            src/packed_chart_read.cpp
            src/packed_profile.cpp
            src/packed_semantic_identity.cpp
            src/compiled_chart_timing.cpp
        PRIVATE
            FILE_SET HEADERS
            BASE_DIRS codec_include
            FILES
                codec_include/cuexis/chart/canonical_semantic_chart.hpp
                codec_include/cuexis/chart/packed_chart_primitives.hpp
                codec_include/cuexis/chart/packed_chart_reader.hpp
                codec_include/cuexis/chart/packed_chart_tables.hpp
    )

    target_include_directories(cuexis_chart_codec
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/codec_include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/internal>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_compile_features(cuexis_chart_codec PUBLIC cxx_std_20)

    target_link_libraries(cuexis_chart_codec
        PUBLIC
            cuexis::chart
            cuexis::core
            cuexis::timing
    )

    add_library(cuexis_chart_tools STATIC)
    add_library(cuexis::chart_tools ALIAS cuexis_chart_tools)
    set_target_properties(cuexis_chart_tools PROPERTIES EXPORT_NAME InternalChartTools)

    # The explicit v5 candidate loader embeds the candidate Chart schema at configure
    # time. The schema file stays the authority; the generated header only carries it into
    # the build-only loader so the two cannot drift apart unnoticed.
    set(CUEXIS_CHART_V5_SCHEMA_FILE
        "${PROJECT_SOURCE_DIR}/schemas/candidate/cuexis.chart.beat-us.v5.schema.json")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CUEXIS_CHART_V5_SCHEMA_FILE}")
    file(READ "${CUEXIS_CHART_V5_SCHEMA_FILE}" CUEXIS_CHART_V5_SCHEMA)
    configure_file(src/chart_v5_schema.hpp.in
        "${CMAKE_CURRENT_BINARY_DIR}/generated/chart_v5_schema.hpp" @ONLY)

    # CXT2 expansion, candidate/Behavior source compilation, candidate assembly, migration and
    # the Packed file Writer. The old Resolver-facing chart_writer.cpp stays in the base target.
    target_sources(cuexis_chart_tools
        PRIVATE
            src/chart_migrator.cpp
            src/chart_v5_loader.cpp
            src/cxt_v2_loader.cpp
            src/cxt_v2_loader_expand.cpp
            src/cxt_v2_loader_parse_nodes.cpp
            src/cxt_v2_loader_parse_values.cpp
            src/cxt_v2_loader_reader.cpp
            src/cxt_v2_loader_units_internal.hpp
            src/candidate_lowering.cpp
            src/candidate_assembler.cpp
            src/behavior_candidate.cpp
            src/chart_v5_timing.cpp
            src/packed_chart_tables_sections.cpp
            src/packed_chart_tables_writer.cpp
            src/packed_chart_io.cpp
        PRIVATE
            FILE_SET HEADERS
            BASE_DIRS tools_include
            FILES
                tools_include/cuexis/chart/behavior_candidate.hpp
                tools_include/cuexis/chart/candidate_assembler.hpp
                tools_include/cuexis/chart/candidate_lowering.hpp
                tools_include/cuexis/chart/chart_migrator.hpp
                tools_include/cuexis/chart/chart_v5_document.hpp
                tools_include/cuexis/chart/chart_v5_timing.hpp
                tools_include/cuexis/chart/cxt_v2_loader.hpp
                tools_include/cuexis/chart/packed_chart_io.hpp
                tools_include/cuexis/chart/packed_chart_writer.hpp
    )

    target_include_directories(cuexis_chart_tools
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tools_include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_BINARY_DIR}/generated
    )

    target_compile_features(cuexis_chart_tools PUBLIC cxx_std_20)

    target_link_libraries(cuexis_chart_tools
        PUBLIC
            cuexis::chart
            cuexis::chart_codec
            cuexis::core
        PRIVATE
            cuexis::json_support
    )
endif()
