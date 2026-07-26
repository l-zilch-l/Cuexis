add_library(cuexis_gameplay INTERFACE)
add_library(cuexis::gameplay ALIAS cuexis_gameplay)

set_target_properties(cuexis_gameplay PROPERTIES EXPORT_NAME InternalGameplay)

target_sources(cuexis_gameplay
    INTERFACE
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/cuexis/gameplay/tags.hpp
)

target_compile_features(cuexis_gameplay INTERFACE cxx_std_20)

target_link_libraries(cuexis_gameplay INTERFACE cuexis::core)
