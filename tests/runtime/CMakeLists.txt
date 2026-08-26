add_executable(cuexis_runtime_tests
    animation_runtime_tests.cpp
    behavior_runtime_tests.cpp
    migration_equivalence_tests.cpp
    runtime_session_tests.cpp
)

target_link_libraries(cuexis_runtime_tests
    PRIVATE
        cuexis::animation
        cuexis::assets
        cuexis::behavior
        cuexis::chart
        cuexis::gameplay
        cuexis::render
        cuexis::runtime
        cuexis::world
        Catch2::Catch2WithMain
)

target_compile_definitions(cuexis_runtime_tests
    PRIVATE
        CUEXIS_TEST_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

include(Catch)
catch_discover_tests(cuexis_runtime_tests)
