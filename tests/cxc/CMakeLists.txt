# The candidate extension and round-trip cases exercise the repository developer tool layer
# (cuexis::cxc_tool_common) rather than the public package API alone. Headless presets do not
# build that layer, so those two sources are added only when the target exists; the package
# reader cases stay registered in every configuration.
set(cuexis_cxc_test_sources
    cfu_f4_safety_tests.cpp
    cxc_binary_fixture_tests.cpp
    cxc_candidate_bridge_tests.cpp
    cxc_package_tests.cpp
    cxc_manifest_loader_tests.cpp
    cxc_test_support.cpp
    cxc_test_support.hpp
    parse_characterization_tests.cpp
    zip32_envelope_tests.cpp
)

if(TARGET cuexis::cxc_tool_common)
    list(APPEND cuexis_cxc_test_sources
        cxc_candidate_extension_tests.cpp
        cxc_candidate_roundtrip_tests.cpp
    )
else()
    message(STATUS "cuexis_cxc_tests: developer tool layer absent; candidate CXC cases excluded")
endif()

add_executable(cuexis_cxc_tests ${cuexis_cxc_test_sources})

target_compile_definitions(cuexis_cxc_tests
    PRIVATE
        CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

target_include_directories(cuexis_cxc_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/engine/cxc/src"
        "${PROJECT_SOURCE_DIR}/engine/json_support/src"
)

target_link_libraries(cuexis_cxc_tests
    PRIVATE
        cuexis_cxc
        cuexis::chart
        Catch2::Catch2WithMain
)

if(TARGET cuexis::cxc_tool_common)
    target_link_libraries(cuexis_cxc_tests
        PRIVATE
            cuexis::cxc_tool_common
            cuexis::chart
    )
endif()

include(Catch)
catch_discover_tests(cuexis_cxc_tests)
