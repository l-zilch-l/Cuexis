add_library(cuexis_assets STATIC
    src/asset_database.cpp
    src/resource_manager.cpp
)
add_library(cuexis::assets ALIAS cuexis_assets)

target_compile_features(cuexis_assets PUBLIC cxx_std_20)

target_include_directories(cuexis_assets
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(cuexis_assets PUBLIC cuexis::core)
