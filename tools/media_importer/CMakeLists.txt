add_executable(cuexis_media_importer
    src/main.cpp
    src/publish.cpp
    src/worker_process.cpp
)
add_executable(cuexis::media_importer ALIAS cuexis_media_importer)

target_compile_features(cuexis_media_importer PRIVATE cxx_std_20)
target_link_libraries(cuexis_media_importer
    PRIVATE
        cuexis::core
        cuexis::media_import
)

if(MSVC)
    target_compile_options(cuexis_media_importer PRIVATE /fp:precise)
else()
    target_compile_options(cuexis_media_importer PRIVATE -fno-fast-math -ffp-contract=off)
endif()

if(CUEXIS_BUILD_TESTS)
    add_test(
        NAME cuexis_media_importer_tool_tests
        COMMAND "${CMAKE_COMMAND}"
            "-DCUEXIS_MEDIA_IMPORTER=$<TARGET_FILE:cuexis_media_importer>"
            "-DCUEXIS_MEDIA_IMPORTER_WORK=$<TARGET_FILE_DIR:cuexis_media_importer>/s6e_importer_work"
            "-DCUEXIS_MEDIA_FIXTURES=${PROJECT_SOURCE_DIR}/tests/fixtures/stage6_e/media"
            -P "${PROJECT_SOURCE_DIR}/cmake/VerifyMediaImporter.cmake"
    )
    set_tests_properties(cuexis_media_importer_tool_tests PROPERTIES LABELS "tools;s6-e")
endif()
