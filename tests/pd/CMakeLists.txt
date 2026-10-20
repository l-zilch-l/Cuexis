# PD probes are independent from RPA acceptance. The allocation probe is static-only.
if(CUEXIS_ENABLE_CHART_V5_CANDIDATE AND CUEXIS_LIBRARY_TYPE STREQUAL "STATIC")
    add_executable(cuexis_pd_baseline_probe pd_baseline_probe.cpp measurement_method_tests.cpp advance_retirement_tests.cpp)
    target_compile_features(cuexis_pd_baseline_probe PRIVATE cxx_std_20)
    target_compile_definitions(cuexis_pd_baseline_probe
        PRIVATE CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    target_link_libraries(cuexis_pd_baseline_probe
        PRIVATE cuexis::playback cuexis::judgement cuexis::gameplay_packed
                Catch2::Catch2WithMain)
    include(Catch)
    catch_discover_tests(cuexis_pd_baseline_probe ADD_TAGS_AS_LABELS PROPERTIES LABELS "pd")
    add_executable(cuexis_pd_allocation_probe measurement_method_tests.cpp)
    target_compile_features(cuexis_pd_allocation_probe PRIVATE cxx_std_20)
    target_compile_definitions(cuexis_pd_allocation_probe PRIVATE
        CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}" CUEXIS_PD_ALLOCATION_OBSERVER=1)
    target_link_libraries(cuexis_pd_allocation_probe PRIVATE cuexis::playback cuexis::judgement
        cuexis::gameplay_packed Catch2::Catch2WithMain)
    if(TARGET cuexis_execution_host_support)
        foreach(probe cuexis_pd_baseline_probe cuexis_pd_allocation_probe)
            target_sources(${probe} PRIVATE delivery_baseline_tests.cpp legacy_frame_observation.cpp)
            target_include_directories(${probe} PRIVATE "${PROJECT_SOURCE_DIR}/app/execution_host")
            target_link_libraries(${probe} PRIVATE cuexis::execution_host_support
                cuexis::presentation_renderer)
        endforeach()
    endif()
    if(TARGET cuexis_execution_host_support)
        add_executable(cuexis_pd_host_probe host_delivery_probe.cpp pd5_host_probe.cpp pd5_matrix_probe.cpp owner_wait_probe.cpp owner_channel_wait_tests.cpp coherent_retirement_tests.cpp
            "${PROJECT_SOURCE_DIR}/app/player/src/execution_host.cpp"
            "${PROJECT_SOURCE_DIR}/app/player/src/snapshot_scene.cpp")
        target_compile_features(cuexis_pd_host_probe PRIVATE cxx_std_20)
        target_compile_definitions(cuexis_pd_host_probe PRIVATE CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
        target_link_libraries(cuexis_pd_host_probe PRIVATE cuexis::execution_host_support
            cuexis::playback cuexis::presentation_renderer cuexis::render cuexis::judgement cuexis::gameplay_packed Catch2::Catch2WithMain)
        catch_discover_tests(cuexis_pd_host_probe ADD_TAGS_AS_LABELS PROPERTIES LABELS "pd")
    endif()
endif()
