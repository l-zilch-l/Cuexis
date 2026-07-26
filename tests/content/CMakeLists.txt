add_executable(cuexis_content_tests
    content_provider_tests.cpp
)

target_link_libraries(cuexis_content_tests
    PRIVATE
        cuexis::content
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_content_tests)
