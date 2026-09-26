add_library(cuexis_asset_publish STATIC)
add_library(cuexis::asset_publish ALIAS cuexis_asset_publish)

target_sources(cuexis_asset_publish
    PRIVATE
        src/asset_publish.cpp
        src/publish_fs_internal.cpp
        src/publish_fs_internal.hpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
        include/cuexis/tools/asset_publish.hpp
)

target_compile_features(cuexis_asset_publish PUBLIC cxx_std_20)
target_link_libraries(cuexis_asset_publish
    PUBLIC
        cuexis::core
        cuexis_cxc
)

# Failure-injection hooks exist only in test builds.
if(CUEXIS_BUILD_TESTS)
    target_compile_definitions(cuexis_asset_publish PRIVATE CUEXIS_ASSET_PUBLISH_TESTING=1)
endif()
