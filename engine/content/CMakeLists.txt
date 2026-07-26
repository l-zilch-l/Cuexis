add_library(cuexis_content STATIC
    src/content_provider.cpp
)
add_library(cuexis::content ALIAS cuexis_content)
add_library(Cuexis::Content ALIAS cuexis_content)

set_target_properties(cuexis_content PROPERTIES EXPORT_NAME Content)

target_sources(cuexis_content
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/cuexis/content/content_provider.hpp
)

target_compile_features(cuexis_content PUBLIC cxx_std_20)

target_link_libraries(cuexis_content
    PUBLIC
        cuexis::core
    PRIVATE
        cuexis::filesystem
)
