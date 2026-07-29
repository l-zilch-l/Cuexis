add_library(cuexis_project STATIC)
add_library(cuexis::project ALIAS cuexis_project)

set_target_properties(cuexis_project PROPERTIES EXPORT_NAME InternalProject)

target_sources(cuexis_project
    PRIVATE
        src/asset_index_reader.cpp
        src/project_validation.hpp
        src/project_loader.cpp
    PRIVATE
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/cuexis/project/asset_index_reader.hpp
            include/cuexis/project/project_config.hpp
            include/cuexis/project/project_loader.hpp
)

target_include_directories(cuexis_project PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(cuexis_project PUBLIC cxx_std_20)

target_link_libraries(cuexis_project
    PUBLIC
        cuexis::core
    PRIVATE
        cuexis::filesystem
        cuexis::json_support
)
