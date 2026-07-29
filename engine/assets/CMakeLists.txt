add_library(cuexis_assets STATIC
    src/asset_database.cpp
    src/resource_manager.cpp
)
add_library(cuexis::assets ALIAS cuexis_assets)

set_target_properties(cuexis_assets PROPERTIES EXPORT_NAME InternalAssets)

target_sources(cuexis_assets
    PRIVATE
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/cuexis/assets/asset_database.hpp
            include/cuexis/assets/asset_id.hpp
            include/cuexis/assets/resource_handle.hpp
            include/cuexis/assets/resource_manager.hpp
)

target_include_directories(cuexis_assets PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(cuexis_assets PUBLIC cxx_std_20)

target_link_libraries(cuexis_assets
    PUBLIC
        cuexis::content
        cuexis::core
)
