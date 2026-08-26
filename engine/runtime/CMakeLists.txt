add_library(cuexis_runtime STATIC
    src/chart_world_instantiator.cpp
    src/runtime_session.cpp
)
add_library(cuexis::runtime ALIAS cuexis_runtime)

set_target_properties(cuexis_runtime PROPERTIES EXPORT_NAME InternalRuntime)

target_sources(cuexis_runtime
    PRIVATE
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/cuexis/runtime/chart_world_instantiator.hpp
            include/cuexis/runtime/runtime_frame.hpp
            include/cuexis/runtime/runtime_session.hpp
)

target_include_directories(cuexis_runtime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(cuexis_runtime PUBLIC cxx_std_20)

target_link_libraries(cuexis_runtime
    PUBLIC
        cuexis::assets
        cuexis::core
        cuexis::chart
        cuexis::world
    PRIVATE
        cuexis::animation
        cuexis::behavior
        cuexis::gameplay
        cuexis::render
)
