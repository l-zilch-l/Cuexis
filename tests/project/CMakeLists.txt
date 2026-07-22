add_executable(cuexis_project_tests
    asset_index_reader_tests.cpp
    project_loader_tests.cpp
)

target_compile_definitions(cuexis_project_tests
    PRIVATE
        CUEXIS_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(cuexis_project_tests
    PRIVATE
        cuexis::project
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_project_tests)
