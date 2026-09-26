add_executable(cuexis_media_import_tests
    media_import_tests.cpp
)

target_compile_features(cuexis_media_import_tests PRIVATE cxx_std_20)
target_compile_definitions(cuexis_media_import_tests
    PRIVATE
        CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_link_libraries(cuexis_media_import_tests
    PRIVATE
        cuexis::core
        cuexis::media_import
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_media_import_tests)
