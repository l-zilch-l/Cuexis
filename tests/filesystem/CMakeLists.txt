add_executable(cuexis_filesystem_tests
    secure_file_tests.cpp
)

target_link_libraries(cuexis_filesystem_tests
    PRIVATE
        cuexis::filesystem
        Catch2::Catch2WithMain
)

target_include_directories(cuexis_filesystem_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/engine/filesystem/src"
)

include(Catch)
catch_discover_tests(cuexis_filesystem_tests)
