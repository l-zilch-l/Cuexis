# The legacy read/write Packed Capsule adapter is part of the same support-library condition as
# the Chart codec/tools and CXC candidate archives: it needs the Packed semantic model and the
# internal Writer, so it must not be created when only the default Playback/Player graph is built.
if(CUEXIS_ENABLE_CHART_V5_CANDIDATE OR CUEXIS_BUILD_DEVELOPER_TOOLS OR
   (BUILD_TESTING AND CUEXIS_BUILD_TESTS))
    add_library(cuexis_gameplay_packed STATIC
        src/gameplay_capsule_adapters_internal.hpp
        src/gameplay_capsule_common_internal.hpp
        src/gameplay_capsule_emit.cpp
        src/gameplay_capsule_lookup_internal.hpp
        src/gameplay_capsule_model.cpp
        src/gameplay_capsule_model_internal.hpp
        src/gameplay_capsule_operations_internal.hpp
        src/gameplay_capsule_read.cpp
        src/gameplay_capsule_relation_internal.hpp
        src/gameplay_capsule_visit_internal.hpp
        src/gameplay_capsule.cpp
    )
    add_library(cuexis::gameplay_packed ALIAS cuexis_gameplay_packed)
    set_target_properties(cuexis_gameplay_packed PROPERTIES EXPORT_NAME InternalGameplayPacked)
    target_compile_features(cuexis_gameplay_packed PUBLIC cxx_std_20)
    target_include_directories(cuexis_gameplay_packed PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )
    target_include_directories(cuexis_gameplay_packed PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../chart/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../judgement/src
    )
    # Offline physical adapter only. No installed headers and no dependency back into Judgement.
    # The public header exposes the codec semantic model; the internal Writer/atomic output stays
    # private to this legacy read/write target.
    target_link_libraries(cuexis_gameplay_packed
        PUBLIC
            cuexis::chart
            cuexis::chart_codec
            cuexis::judgement
            cuexis::filesystem
            cuexis::core
        PRIVATE
            cuexis::chart_tools
            cuexis::json_support
    )
endif()
