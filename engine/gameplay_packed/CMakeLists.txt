add_library(cuexis_gameplay_packed STATIC src/gameplay_capsule.cpp)
add_library(cuexis::gameplay_packed ALIAS cuexis_gameplay_packed)
target_compile_features(cuexis_gameplay_packed PUBLIC cxx_std_20)
target_include_directories(cuexis_gameplay_packed PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_include_directories(cuexis_gameplay_packed PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../chart/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../judgement/src
)
# Offline physical adapter only. No installed headers and no dependency back into Judgement.
target_link_libraries(cuexis_gameplay_packed PUBLIC
    cuexis::chart cuexis::judgement cuexis::filesystem cuexis::core
)
target_link_libraries(cuexis_gameplay_packed PRIVATE cuexis::json_support)
