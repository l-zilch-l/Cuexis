add_library(cuexis_gameplay_author STATIC src/gameplay_author.cpp)
add_library(cuexis::gameplay_author ALIAS cuexis_gameplay_author)
target_include_directories(cuexis_gameplay_author PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(cuexis_gameplay_author PUBLIC cxx_std_20)
target_link_libraries(cuexis_gameplay_author PUBLIC
    cuexis::core cuexis::json_support cuexis::chart cuexis::judgement cuexis::gameplay_packed)
