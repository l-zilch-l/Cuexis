add_executable(cuexis_cxc_tests
    cfu_f4_safety_tests.cpp
    cxc_binary_fixture_tests.cpp
    cxc_package_tests.cpp
    cxc_manifest_loader_tests.cpp
    cxc_test_support.cpp
    cxc_test_support.hpp
    parse_characterization_tests.cpp
    zip32_envelope_tests.cpp
)

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
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_cxc_tests)
