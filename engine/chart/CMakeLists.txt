add_library(cuexis_chart STATIC)
add_library(cuexis::chart ALIAS cuexis_chart)

set_target_properties(cuexis_chart PROPERTIES EXPORT_NAME InternalChart)

target_sources(cuexis_chart
    PRIVATE
        src/canonical_chart_loader.cpp
        src/chart_loader.cpp
        src/chart_migrator.cpp
        src/chart_runtime.cpp
        src/rational_beat.cpp
        src/timing_map.cpp
        src/uuid.cpp
    PRIVATE
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/cuexis/chart/canonical_chart_loader.hpp
            include/cuexis/chart/chart_document.hpp
            include/cuexis/chart/chart_loader.hpp
            include/cuexis/chart/chart_migrator.hpp
            include/cuexis/chart/chart_runtime.hpp
            include/cuexis/chart/limits.hpp
            include/cuexis/chart/rational_beat.hpp
            include/cuexis/chart/timing_map.hpp
            include/cuexis/chart/uuid.hpp
)

target_include_directories(cuexis_chart PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(cuexis_chart PUBLIC cxx_std_20)

target_link_libraries(cuexis_chart
    PUBLIC
        cuexis::core
    PRIVATE
        cuexis::json_support
)
