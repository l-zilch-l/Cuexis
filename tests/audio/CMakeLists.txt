add_executable(cuexis_audio_tests
    audio_tests.cpp
    fake_audio_transport_tests.cpp
)
target_compile_features(cuexis_audio_tests PRIVATE cxx_std_20)
target_link_libraries(cuexis_audio_tests PRIVATE cuexis::audio Catch2::Catch2WithMain)
include(Catch)
catch_discover_tests(cuexis_audio_tests)
