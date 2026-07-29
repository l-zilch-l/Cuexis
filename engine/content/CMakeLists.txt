add_library(cuexis_content ${CUEXIS_LIBRARY_TYPE}
    src/content_provider.cpp
)
add_library(cuexis::content ALIAS cuexis_content)
add_library(Cuexis::Content ALIAS cuexis_content)

generate_export_header(cuexis_content
    BASE_NAME CUEXIS_CONTENT
    EXPORT_MACRO_NAME CUEXIS_CONTENT_API
    EXPORT_FILE_NAME "${PROJECT_BINARY_DIR}/generated/cuexis/content/content_export.hpp"
)
if(CUEXIS_LIBRARY_TYPE STREQUAL "STATIC")
    target_compile_definitions(cuexis_content PUBLIC CUEXIS_CONTENT_STATIC_DEFINE)
else()
    target_compile_definitions(cuexis_content PRIVATE CUEXIS_CONTENT_EXPORTS)
endif()

set_target_properties(cuexis_content PROPERTIES EXPORT_NAME Content)
cuexis_configure_public_library(cuexis_content cuexis_content)

target_sources(cuexis_content
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include "${PROJECT_BINARY_DIR}/generated"
        FILES
            "${PROJECT_BINARY_DIR}/generated/cuexis/content/content_export.hpp"
            include/cuexis/content/content_provider.hpp
)

target_compile_features(cuexis_content PUBLIC cxx_std_20)

target_link_libraries(cuexis_content
    PUBLIC
        cuexis::core
    PRIVATE
        cuexis::filesystem
)
