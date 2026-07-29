add_library(cuexis_filesystem STATIC
    src/secure_file.cpp
    src/secure_file_test_support.hpp
)
add_library(cuexis::filesystem ALIAS cuexis_filesystem)

set_target_properties(cuexis_filesystem PROPERTIES EXPORT_NAME InternalFilesystem)

target_sources(cuexis_filesystem
    PRIVATE
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/cuexis/filesystem/secure_file.hpp
)

target_include_directories(cuexis_filesystem PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(cuexis_filesystem PUBLIC cxx_std_20)

target_link_libraries(cuexis_filesystem PUBLIC cuexis::core)
