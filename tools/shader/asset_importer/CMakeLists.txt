add_executable(cuexis_asset_importer main.cpp)

target_compile_features(cuexis_asset_importer PRIVATE cxx_std_20)
target_link_libraries(cuexis_asset_importer PRIVATE cuexis::shader)

if(CUEXIS_BUILD_TESTS)
    add_test(
        NAME cuexis_asset_importer_tool_tests
        COMMAND "${CMAKE_COMMAND}"
            "-DCUEXIS_ASSET_IMPORTER=$<TARGET_FILE:cuexis_asset_importer>"
            "-DCUEXIS_ASSET_IMPORTER_WORK=$<TARGET_FILE_DIR:cuexis_asset_importer>/s5f_importer_work"
            -P "${PROJECT_SOURCE_DIR}/cmake/VerifyAssetImporter.cmake"
    )
    set_tests_properties(cuexis_asset_importer_tool_tests PROPERTIES LABELS "tools;s5-d;s5-f")
endif()
