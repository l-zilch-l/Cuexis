add_library(cuexis_timing STATIC)
add_library(cuexis::timing ALIAS cuexis_timing)
set_target_properties(cuexis_timing PROPERTIES EXPORT_NAME InternalTiming)

# Single beat <-> microsecond mapping shared by the author compiler and the compiled artifact
# reader. Internal build-only archive: the include root is a BUILD_INTERFACE and no header is
# installed.
target_sources(cuexis_timing
    PRIVATE
        src/rational_beat.cpp
        src/timing_map.cpp
)

target_include_directories(cuexis_timing PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(cuexis_timing PUBLIC cxx_std_20)

target_link_libraries(cuexis_timing
    PUBLIC
        cuexis::core
)
