add_executable(cuexis_json_support_tests
    parse_tests.cpp
    reader_tests.cpp
    schema_artifact_tests.cpp
    schema_tests.cpp
)

target_compile_definitions(cuexis_json_support_tests
    PRIVATE
        CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

target_link_libraries(cuexis_json_support_tests
    PRIVATE
        cuexis::json_support
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_json_support_tests)
