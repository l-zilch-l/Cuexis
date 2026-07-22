find_package(nlohmann_json CONFIG REQUIRED)
find_package(nlohmann_json_schema_validator CONFIG REQUIRED)

add_library(cuexis_json_support STATIC)
add_library(cuexis::json_support ALIAS cuexis_json_support)

target_sources(cuexis_json_support
    PRIVATE
        src/json_conversion.hpp
        src/parse.cpp
        src/reader.cpp
        src/schema.cpp
        src/value.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/cuexis/json/parse.hpp
            include/cuexis/json/reader.hpp
            include/cuexis/json/schema.hpp
            include/cuexis/json/value.hpp
)

target_compile_features(cuexis_json_support PUBLIC cxx_std_20)

target_link_libraries(cuexis_json_support
    PUBLIC
        cuexis::core
    PRIVATE
        nlohmann_json::nlohmann_json
        nlohmann_json_schema_validator::validator
)
