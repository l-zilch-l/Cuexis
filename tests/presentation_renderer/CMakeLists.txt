add_executable(cuexis_presentation_renderer_tests
    presentation_renderer_tests.cpp
)

target_compile_features(cuexis_presentation_renderer_tests PRIVATE cxx_std_20)
target_compile_definitions(cuexis_presentation_renderer_tests
    PRIVATE
        CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_link_libraries(cuexis_presentation_renderer_tests
    PRIVATE
        cuexis::presentation_renderer
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_presentation_renderer_tests)
