find_package(EnTT CONFIG REQUIRED)

add_library(cuexis_world STATIC
    src/property.cpp
    src/transform_system.cpp
    src/world.cpp
)
add_library(cuexis::world ALIAS cuexis_world)

target_compile_features(cuexis_world PUBLIC cxx_std_20)

target_include_directories(cuexis_world
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(cuexis_world
    PUBLIC
        cuexis::core
        EnTT::EnTT
)
