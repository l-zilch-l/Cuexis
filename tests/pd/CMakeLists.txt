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
        target_compile_definitions(cuexis_pd_host_probe PRIVATE
            CUEXIS_PD5_CAPACITY_SOURCE_SHA="${CUEXIS_IMPLEMENTATION_SHA}")
        target_link_libraries(cuexis_pd_host_probe PRIVATE cuexis::execution_host_support
            cuexis::playback cuexis::presentation_renderer cuexis::render cuexis::judgement cuexis::gameplay_packed Catch2::Catch2WithMain)
        catch_discover_tests(cuexis_pd_host_probe ADD_TAGS_AS_LABELS PROPERTIES LABELS "pd")
    endif()
endif()

# Offline tool contracts do not launch WPR, ETW roles, or Cuexis measurements.
if(CUEXIS_ENABLE_CHART_V5_CANDIDATE)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(pd5_tool_test_tmp "${PROJECT_BINARY_DIR}/pd5-tool-test-tmp")
    file(MAKE_DIRECTORY "${pd5_tool_test_tmp}")
    set(pd5_tool_suites
            pd5_etw_collect_f1_parse_tests
            pd5_memory_domains_tests
            pd5_tool_contract_tests
            pd5_admission_feasibility_tests
            test_gameplay_admission_evidence)
    # Controller/Job tests require Windows process APIs and the ETW x64 ABI.
    if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
        list(APPEND pd5_tool_suites
            pd5_etw_controller_tests
            pd5_etw_f1_scale_tests
            pd5_etw_flow_tests
            pd5_etw_job_tests)
    endif()
    foreach(suite IN LISTS pd5_tool_suites)
        add_test(NAME "cuexis_${suite}"
            COMMAND "${Python3_EXECUTABLE}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/${suite}.py")
        set_tests_properties("cuexis_${suite}" PROPERTIES
            LABELS "pd;tools;contract"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            RESOURCE_LOCK cuexis_pd5_tool_processes
            TIMEOUT 300
            ENVIRONMENT
                "PYTHONIOENCODING=utf-8;PYTHONDONTWRITEBYTECODE=1;TMPDIR=${pd5_tool_test_tmp};TEMP=${pd5_tool_test_tmp};TMP=${pd5_tool_test_tmp}")
    endforeach()
endif()
