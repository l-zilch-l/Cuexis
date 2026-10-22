add_executable(cuexis_gameplay_packed_tests gameplay_capsule_tests.cpp hash_work_tests.cpp hash_core_tests.cpp)
target_compile_features(cuexis_gameplay_packed_tests PRIVATE cxx_std_20)
target_include_directories(cuexis_gameplay_packed_tests PRIVATE
    ../judgement
    ../chart
    ${PROJECT_SOURCE_DIR}/engine/chart/src
    ${PROJECT_SOURCE_DIR}/engine/gameplay_packed/src
)
target_link_libraries(cuexis_gameplay_packed_tests PRIVATE
    cuexis::gameplay_packed
    cuexis::json_support
    Catch2::Catch2WithMain
)
include(Catch)
catch_discover_tests(cuexis_gameplay_packed_tests PROPERTIES LABELS "gameplay_packed")
