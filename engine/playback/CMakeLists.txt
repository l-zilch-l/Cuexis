add_library(cuexis_playback ${CUEXIS_LIBRARY_TYPE}
    src/frame_digest.cpp
    src/frame_digest_internal.hpp
    src/frame_digest_version.cpp
    src/frame_layout.cpp
    src/frame_layout_bridge.cpp
    src/frame_layout_internal.hpp
    src/presentation.cpp
    src/presentation_identity.cpp
    src/presentation_parse.cpp
    src/presentation_reader.cpp
    src/presentation_units_internal.hpp
    src/presentation_extraction.cpp
    src/presentation_extraction.hpp
    src/presentation_internal.hpp
    src/playback_candidate_internal.hpp
    src/playback_session.cpp
    src/playback_session_internal.hpp
    src/playback_session_prepare.cpp
    src/playback_session_prepared.cpp
    src/playback_session_prepare_capabilities.cpp
    src/playback_session_prepare_internal.hpp
    src/playback_session_prepare_stages.cpp
    src/presentation_materializer.cpp
    src/playback_source.cpp
    src/playback_source_internal.hpp
    src/playback_source_state.hpp
    src/playback_source_support.cpp
    src/runtime_timeline.cpp
    src/realtime_clock.cpp
)
add_library(cuexis::playback ALIAS cuexis_playback)
add_library(Cuexis::Playback ALIAS cuexis_playback)

generate_export_header(cuexis_playback
    BASE_NAME CUEXIS_PLAYBACK
    EXPORT_MACRO_NAME CUEXIS_PLAYBACK_API
    EXPORT_FILE_NAME "${PROJECT_BINARY_DIR}/generated/cuexis/playback/playback_export.hpp"
)
if(CUEXIS_LIBRARY_TYPE STREQUAL "STATIC")
    target_compile_definitions(cuexis_playback PUBLIC CUEXIS_PLAYBACK_STATIC_DEFINE)
else()
    target_compile_definitions(cuexis_playback PRIVATE CUEXIS_PLAYBACK_EXPORTS)
endif()
if(CUEXIS_ENABLE_CHART_V5_CANDIDATE)
    target_compile_definitions(cuexis_playback PRIVATE CUEXIS_ENABLE_CHART_V5_CANDIDATE=1)
    target_compile_definitions(cuexis_playback PUBLIC CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE=1)
    target_include_directories(cuexis_playback PRIVATE ${PROJECT_SOURCE_DIR}/engine/judgement/internal)
    # Experimental R5 frame construction surface. Only the opt-in flavor publishes the same
    # physical header through the SDK header file set and the build interface; the default flavor
    # keeps its PRIVATE include root and its installed surface unchanged.
    target_sources(cuexis_playback PUBLIC
        FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/experimental_include"
        FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/experimental_include/cuexis/playback/experimental/frame_layout.hpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/experimental_include/cuexis/playback/experimental/gameplay.hpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/experimental_include/cuexis/playback/experimental/gameplay_realtime.hpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/experimental_include/cuexis/playback/experimental/gameplay_replay.hpp"
    )
    target_include_directories(cuexis_playback PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/experimental_include>")
    # Candidate-only implementation units. The ordinary source table above never compiles them;
    # each hook therefore has exactly one definition in either closure, never both. The codec and
    # tools include roots arrive only through this candidate-only link edge.
    target_sources(cuexis_playback PRIVATE
        src/gameplay_candidate.cpp
        src/gameplay_configuration.cpp
        src/gameplay_entry.cpp
        src/playback_session_execution.cpp
        src/playback_session_execution_host_access.cpp
        src/playback_session_gameplay.cpp
        src/playback_session_gameplay_hooks.cpp
        src/playback_session_gameplay_seek.cpp
        src/playback_session_prepare_gameplay.cpp
        src/playback_session_realtime.cpp
        src/playback_source_gameplay.cpp
        src/presentation_projection.cpp
        src/presentation_projection.hpp)
    target_link_libraries(cuexis_playback PRIVATE cuexis::judgement cuexis::gameplay_packed
        cuexis::cxc_candidate cuexis::chart_codec cuexis::chart_tools cuexis::json_support)
endif()

set_target_properties(cuexis_playback PROPERTIES EXPORT_NAME Playback)
cuexis_configure_public_library(cuexis_playback cuexis_playback)

target_sources(cuexis_playback
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include "${PROJECT_BINARY_DIR}/generated"
        FILES
            "${PROJECT_BINARY_DIR}/generated/cuexis/playback/playback_export.hpp"
            include/cuexis/playback/content_provider.hpp
            include/cuexis/playback/frame_digest.hpp
            include/cuexis/playback/playback_session.hpp
            include/cuexis/playback/playback_source.hpp
            include/cuexis/playback/presentation.hpp
            include/cuexis/playback/runtime_timeline.hpp
            include/cuexis/playback/realtime_clock.hpp
            include/cuexis/playback/playback_types.hpp
            include/cuexis/playback/presentation_candidate.hpp
)

target_compile_features(cuexis_playback PUBLIC cxx_std_20)
if(CUEXIS_ENABLE_CHART_V5_CANDIDATE)
    target_sources(cuexis_playback PUBLIC FILE_SET HEADERS FILES
        include/cuexis/playback/gameplay_candidate.hpp
        include/cuexis/playback/execution_coordination.hpp
        include/cuexis/playback/execution_control.hpp)
endif()

target_link_libraries(cuexis_playback
    PUBLIC
        cuexis::audio
        cuexis::content
        cuexis::core
    PRIVATE
        cuexis::animation
        cuexis::assets
        cuexis::chart
        cuexis::cxc
        cuexis::filesystem
        cuexis::project
        cuexis::runtime
        cuexis::render
        cuexis::world
)

target_include_directories(cuexis_playback PRIVATE "${PROJECT_SOURCE_DIR}/engine/runtime/internal")
# Build-only internal root for the execution-host access bridge and the replay identity entry.
# It is never part of the installed include tree or the SDK header file set.
target_include_directories(cuexis_playback PRIVATE
    "${PROJECT_SOURCE_DIR}/engine/playback/internal")
# The experimental frame layout header is consumed only by compiled Playback translation units in
# the default build. Experimental SDK packaging adds the same root to its installed header set.
target_include_directories(cuexis_playback PRIVATE
    "${PROJECT_SOURCE_DIR}/engine/playback/experimental_include")
