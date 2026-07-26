add_executable(cuexis_core_tests
    diagnostic_tests.cpp
    error_tests.cpp
    log_sink_tests.cpp
    math_tests.cpp
    result_tests.cpp
    thread_checker_tests.cpp
    uuid_tests.cpp
    version_tests.cpp
)

target_link_libraries(cuexis_core_tests
    PRIVATE
        cuexis::core
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_core_tests)
