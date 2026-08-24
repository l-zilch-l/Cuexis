find_package(minizip-ng CONFIG REQUIRED)

add_library(cuexis_cxc STATIC)
add_library(cuexis::cxc ALIAS cuexis_cxc)

set_target_properties(cuexis_cxc PROPERTIES EXPORT_NAME InternalCxc)

target_sources(cuexis_cxc
    PRIVATE
        src/cxc_hash_internal.cpp
        src/cxc_manifest_loader.cpp
        src/cxc_package.cpp
        src/cxc_path_internal.cpp
        src/cxc_writer.cpp
        src/zip32_envelope_internal.cpp
        src/cxc_hash_internal.hpp
        src/cxc_path_internal.hpp
        src/zip32_envelope_internal.hpp
    PRIVATE
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
        include/cuexis/cxc/cxc_package.hpp
        include/cuexis/cxc/cxc_writer.hpp
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
        cuexis::content
        cuexis::project
        cuexis::chart
    PRIVATE
        cuexis::filesystem
        cuexis::json_support
        MINIZIP::minizip-ng
)
