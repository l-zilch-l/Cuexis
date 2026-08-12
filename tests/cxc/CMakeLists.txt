add_executable(cuexis_cxc_tests
    cxc_manifest_loader_tests.cpp
)

target_compile_definitions(cuexis_cxc_tests
    PRIVATE
        CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

target_link_libraries(cuexis_cxc_tests
    PRIVATE
        cuexis_cxc
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_cxc_tests)
