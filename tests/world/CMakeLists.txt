add_executable(cuexis_world_tests
    property_tests.cpp
    world_tests.cpp
)

target_link_libraries(cuexis_world_tests
    PRIVATE
        cuexis::world
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_world_tests)
