add_executable(cuexis_chart_candidate main.cpp)
target_compile_features(cuexis_chart_candidate PRIVATE cxx_std_20)
target_include_directories(cuexis_chart_candidate PRIVATE "${PROJECT_SOURCE_DIR}/engine/core/internal")
target_link_libraries(cuexis_chart_candidate PRIVATE
    cuexis::chart cuexis::cxc cuexis::core cuexis::json_support cuexis::asset_publish
    cuexis::playback)

if(CUEXIS_ENABLE_CHART_V5_CANDIDATE)
    target_compile_definitions(cuexis_chart_candidate PRIVATE CUEXIS_GAMEPLAY_ASSEMBLER_CANDIDATE=1)
    target_link_libraries(cuexis_chart_candidate PRIVATE cuexis::gameplay_author)
endif()
