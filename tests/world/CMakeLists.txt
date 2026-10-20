add_executable(cuexis_world_tests
    property_tests.cpp
    world_tests.cpp
    ../pd/world_transaction_tests.cpp
)
target_include_directories(cuexis_world_tests PRIVATE "${PROJECT_SOURCE_DIR}/engine/world/internal")

target_link_libraries(cuexis_world_tests
    PRIVATE
        cuexis::world
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_world_tests)
