add_library(cuexis_debug STATIC)
add_library(cuexis::debug ALIAS cuexis_debug)

target_sources(cuexis_debug
    PRIVATE
        src/debug_draw.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/cuexis/debug/debug_draw.hpp
)

target_compile_features(cuexis_debug PUBLIC cxx_std_20)
target_link_libraries(cuexis_debug
    PUBLIC
        cuexis::core
        cuexis::render
        cuexis::world
)
