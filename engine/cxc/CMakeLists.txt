find_package(minizip-ng CONFIG REQUIRED)

add_library(cuexis_cxc STATIC)
add_library(cuexis::cxc ALIAS cuexis_cxc)

set_target_properties(cuexis_cxc PROPERTIES EXPORT_NAME InternalCxc)

target_sources(cuexis_cxc
    PRIVATE
        src/cxc_hash_internal.cpp
        src/cxc_manifest_loader.cpp
        src/cxc_package.cpp
        src/cxc_package_helpers.hpp
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

# Chart v5 candidate package decoding is a separate internal archive. The general CXC target
# keeps the CXC v1 envelope, path, content and resource-integrity checks and only reads
# candidate entry bytes; candidate selection and semantic validation live here. The dependency
# direction is candidate -> cxc, never the reverse.
if(CUEXIS_ENABLE_CHART_V5_CANDIDATE OR CUEXIS_BUILD_DEVELOPER_TOOLS OR
   (BUILD_TESTING AND CUEXIS_BUILD_TESTS))
    add_library(cuexis_cxc_candidate STATIC)
    add_library(cuexis::cxc_candidate ALIAS cuexis_cxc_candidate)

    set_target_properties(cuexis_cxc_candidate PROPERTIES EXPORT_NAME InternalCxcCandidate)

    target_sources(cuexis_cxc_candidate
        PRIVATE
            src/cxc_candidate.cpp
        PRIVATE
            FILE_SET HEADERS
            BASE_DIRS include
            FILES
                include/cuexis/cxc/cxc_candidate.hpp
    )

    target_include_directories(cuexis_cxc_candidate
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PRIVATE
            src
    )

    target_compile_features(cuexis_cxc_candidate PUBLIC cxx_std_20)

    target_link_libraries(cuexis_cxc_candidate
        PUBLIC
            cuexis::cxc
            cuexis::chart
            cuexis::chart_codec
            cuexis::chart_tools
            cuexis::core
        PRIVATE
            cuexis::json_support
    )
endif()
