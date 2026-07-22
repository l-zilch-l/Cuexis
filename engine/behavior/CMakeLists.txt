add_library(cuexis_behavior STATIC
    src/behavior_system.cpp
)
add_library(cuexis::behavior ALIAS cuexis_behavior)

target_compile_features(cuexis_behavior PUBLIC cxx_std_20)

target_include_directories(cuexis_behavior
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(cuexis_behavior
    PUBLIC
        cuexis::core
        cuexis::world
)
