add_executable(cuexis_assets_tests
    asset_database_tests.cpp
    asset_types_tests.cpp
    resource_manager_tests.cpp
)

target_link_libraries(cuexis_assets_tests
    PRIVATE
        cuexis::assets
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_assets_tests)
