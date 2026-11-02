add_executable(cuexis_presentation_validation_tests
    validation_sink.cpp
    validation_sink_canonical.hpp
    validation_sink_common.hpp
    validation_sink_digest.hpp
    validation_sink_frame.cpp
    validation_sink_model.hpp
    validation_sink_tests.cpp
)

target_compile_definitions(cuexis_presentation_validation_tests
    PRIVATE
        CUEXIS_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

target_link_libraries(cuexis_presentation_validation_tests
    PRIVATE
        cuexis::playback
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(cuexis_presentation_validation_tests)
