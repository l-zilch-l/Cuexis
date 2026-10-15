add_executable(cuexis_rpa_tests
    rpa_b1_fixture.cpp
    rpa_b1_tests.cpp
    rpa_b2_fixture.cpp
    rpa_b2_tests.cpp
    rpa_b3_tests.cpp
    rpa_b3_clock_tests.cpp
    ../judgement/execution_reference.cpp
)

target_compile_features(cuexis_rpa_tests PRIVATE cxx_std_20)
target_compile_definitions(cuexis_rpa_tests PRIVATE CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(cuexis_rpa_tests
    PRIVATE
        cuexis::playback
        cuexis::judgement
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_rpa_tests ADD_TAGS_AS_LABELS PROPERTIES LABELS "rpa")

if(CUEXIS_ENABLE_CHART_V5_CANDIDATE)
# Owner/resource tests have a separate executable from the baseline oracle.
add_executable(cuexis_host_protocol_tests rpa_b4_tests.cpp execution_boundary_tests.cpp)
target_compile_features(cuexis_host_protocol_tests PRIVATE cxx_std_20)
target_compile_definitions(cuexis_host_protocol_tests PRIVATE CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(cuexis_host_protocol_tests PRIVATE cuexis::execution_host_support cuexis::playback cuexis::presentation_renderer Catch2::Catch2WithMain)
catch_discover_tests(cuexis_host_protocol_tests ADD_TAGS_AS_LABELS PROPERTIES LABELS "rpa")

add_executable(cuexis_execution_host_tests
    rpa_b5_failure_tests.cpp
    rpa_b5_audio_tests.cpp
    rpa_b5_session_tests.cpp
    rpa_b5_capture_tests.cpp
    rpa_b5_host_tests.cpp
    "${PROJECT_SOURCE_DIR}/app/player/src/execution_host.cpp"
    "${PROJECT_SOURCE_DIR}/app/player/src/snapshot_scene.cpp"
)
target_compile_features(cuexis_execution_host_tests PRIVATE cxx_std_20)
target_compile_definitions(cuexis_execution_host_tests PRIVATE CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(cuexis_execution_host_tests PRIVATE cuexis::execution_host_support cuexis::playback cuexis::presentation_renderer cuexis::render cuexis::judgement cuexis::gameplay_packed Catch2::Catch2WithMain)
catch_discover_tests(cuexis_execution_host_tests ADD_TAGS_AS_LABELS PROPERTIES LABELS "execution-host")
endif()
