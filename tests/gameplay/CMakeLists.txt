add_executable(cuexis_gameplay_tests
    gameplay_tag_tests.cpp
)

target_link_libraries(cuexis_gameplay_tests
    PRIVATE
        cuexis::gameplay
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_gameplay_tests)
