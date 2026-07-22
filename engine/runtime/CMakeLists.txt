add_library(cuexis_runtime STATIC
    src/chart_world_instantiator.cpp
    src/runtime_session.cpp
)
add_library(cuexis::runtime ALIAS cuexis_runtime)

target_compile_features(cuexis_runtime PUBLIC cxx_std_20)

target_include_directories(cuexis_runtime
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(cuexis_runtime
    PUBLIC
        cuexis::assets
        cuexis::core
        cuexis::chart
        cuexis::world
    PRIVATE
        cuexis::behavior
        cuexis::gameplay
        cuexis::render
)
