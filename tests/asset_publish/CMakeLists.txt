add_executable(cuexis_asset_publish_tests
    asset_publish_tests.cpp
)

target_compile_features(cuexis_asset_publish_tests PRIVATE cxx_std_20)
target_compile_definitions(cuexis_asset_publish_tests
    PRIVATE
        CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        # The inter-process lock case re-invokes this same binary as a lock holder.
        CUEXIS_ASSET_PUBLISH_TEST_BINARY="$<TARGET_FILE:cuexis_asset_publish_tests>"
)
target_link_libraries(cuexis_asset_publish_tests
    PRIVATE
        cuexis::asset_publish
        cuexis::chart
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_asset_publish_tests)
