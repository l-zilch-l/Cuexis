add_library(cuexis_cxc STATIC)

set_target_properties(cuexis_cxc PROPERTIES EXPORT_NAME InternalCxc)

target_sources(cuexis_cxc
    PRIVATE
        src/cxc_manifest_loader.cpp
        include/cuexis/cxc/cxc_manifest.hpp
        include/cuexis/cxc/cxc_manifest_loader.hpp
)

target_include_directories(cuexis_cxc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(cuexis_cxc PUBLIC cxx_std_20)

target_link_libraries(cuexis_cxc
    PUBLIC
        cuexis::core
    PRIVATE
        cuexis::json_support
)
