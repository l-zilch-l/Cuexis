cmake_minimum_required(VERSION 3.25)

project(CuexisBeatUsCompileConsumer LANGUAGES CXX)

if(NOT DEFINED CUEXIS_SOURCE_DIR)
    message(FATAL_ERROR "CUEXIS_SOURCE_DIR is required")
endif()

set(CUEXIS_ENABLE_CHART_V5_CANDIDATE ON CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_PLAYER OFF CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_SDL_ADAPTER OFF CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_AUDIO_SDL_ADAPTER OFF CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_OPENGL_ADAPTER OFF CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_DEVELOPER_TOOLS OFF CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_MEDIA_TOOLS OFF CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_SHADER_TOOLS OFF CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

add_subdirectory("${CUEXIS_SOURCE_DIR}" cuexis)

add_executable(rr_beat_us_compile_consumer main.cpp)
target_compile_features(rr_beat_us_compile_consumer PRIVATE cxx_std_20)
#  The source compile consumer names only the compile side and the shared timing map. Chart,
#  Chart tools, the Packed Reader, Playback and the author tools are not linked here.
target_link_libraries(rr_beat_us_compile_consumer PRIVATE Cuexis::Core cuexis::timing
                                                     cuexis::judgement_compile)

#  Negative translation units, each built in its own configure run. They must fail because the
#  header is not visible from this consumer's include roots:
#
#    - `chart`:  the base Chart header is not a compile-side include root
#    - `reader`: the compiled-artifact Reader header belongs to the future Reader target
set(CUEXIS_BEAT_US_CONSUMER_NEGATIVE "" CACHE STRING
    "Negative translation unit to build: chart or reader")
if(CUEXIS_BEAT_US_CONSUMER_NEGATIVE STREQUAL "chart")
    add_executable(rr_beat_us_compile_negative_chart negative_chart.cpp)
    target_compile_features(rr_beat_us_compile_negative_chart PRIVATE cxx_std_20)
    target_link_libraries(rr_beat_us_compile_negative_chart PRIVATE Cuexis::Core cuexis::timing
                                                                  cuexis::judgement_compile)
elseif(CUEXIS_BEAT_US_CONSUMER_NEGATIVE STREQUAL "reader")
    add_executable(rr_beat_us_compile_negative_reader negative_reader.cpp)
    target_compile_features(rr_beat_us_compile_negative_reader PRIVATE cxx_std_20)
    target_link_libraries(rr_beat_us_compile_negative_reader PRIVATE Cuexis::Core cuexis::timing
                                                                   cuexis::judgement_compile)
elseif(NOT CUEXIS_BEAT_US_CONSUMER_NEGATIVE STREQUAL "")
    message(FATAL_ERROR "Unknown CUEXIS_BEAT_US_CONSUMER_NEGATIVE value: "
                        "${CUEXIS_BEAT_US_CONSUMER_NEGATIVE}")
endif()
