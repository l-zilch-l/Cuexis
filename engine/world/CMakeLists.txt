find_package(EnTT CONFIG REQUIRED)

add_library(cuexis_world STATIC
    src/property.cpp
    src/transform_system.cpp
    src/world.cpp
)
add_library(cuexis::world ALIAS cuexis_world)

set_target_properties(cuexis_world PROPERTIES EXPORT_NAME InternalWorld)

target_sources(cuexis_world
    PRIVATE
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/cuexis/world/components.hpp
            include/cuexis/world/property.hpp
            include/cuexis/world/transform_system.hpp
            include/cuexis/world/world.hpp
)

target_include_directories(cuexis_world PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(cuexis_world PUBLIC cxx_std_20)

target_link_libraries(cuexis_world
    PUBLIC
        cuexis::core
        EnTT::EnTT
)
