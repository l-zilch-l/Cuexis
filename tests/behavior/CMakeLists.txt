add_executable(cuexis_behavior_tests
    behavior_system_tests.cpp
    behavior_component_tests.cpp
)

target_link_libraries(cuexis_behavior_tests
    PRIVATE
        cuexis::behavior
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_behavior_tests)
