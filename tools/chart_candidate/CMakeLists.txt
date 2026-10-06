add_executable(cuexis_chart_candidate main.cpp)
target_compile_features(cuexis_chart_candidate PRIVATE cxx_std_20)
target_include_directories(cuexis_chart_candidate PRIVATE "${PROJECT_SOURCE_DIR}/engine/core/internal")
target_link_libraries(cuexis_chart_candidate PRIVATE
    cuexis::chart cuexis::cxc cuexis::core cuexis::json_support cuexis::asset_publish
    cuexis::playback)
