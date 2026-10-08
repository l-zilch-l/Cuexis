add_library(cuexis_gameplay_author STATIC src/gameplay_author.cpp)
add_library(cuexis::gameplay_author ALIAS cuexis_gameplay_author)
target_include_directories(cuexis_gameplay_author PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(cuexis_gameplay_author PUBLIC cxx_std_20)
target_link_libraries(cuexis_gameplay_author PUBLIC
    cuexis::core cuexis::json_support cuexis::chart cuexis::judgement cuexis::gameplay_packed)

if(CUEXIS_ENABLE_CHART_V5_CANDIDATE)
    target_sources(cuexis_gameplay_author PRIVATE src/assembler_cli.cpp
        "${PROJECT_SOURCE_DIR}/engine/playback/src/gameplay_entry.cpp")
    target_include_directories(cuexis_gameplay_author PRIVATE
        "${PROJECT_SOURCE_DIR}/engine/core/internal" "${PROJECT_SOURCE_DIR}/engine/playback/src")
    target_link_libraries(cuexis_gameplay_author PRIVATE cuexis::playback cuexis::cxc cuexis::asset_publish)
    add_executable(cuexis_gameplay_assemble src/main.cpp)
    target_compile_features(cuexis_gameplay_assemble PRIVATE cxx_std_20)
    target_link_libraries(cuexis_gameplay_assemble PRIVATE cuexis::gameplay_author)
endif()
