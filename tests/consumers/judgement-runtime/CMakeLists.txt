cmake_minimum_required(VERSION 3.25)

project(CuexisJudgementRuntimeConsumer LANGUAGES CXX)

if(NOT DEFINED CUEXIS_SOURCE_DIR)
    message(FATAL_ERROR "CUEXIS_SOURCE_DIR is required")
endif()

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

add_executable(rr_judgement_runtime main.cpp)
target_compile_features(rr_judgement_runtime PRIVATE cxx_std_20)
# The runtime consumer sees Core and Judgement only: no Playback, Chart, CXC, gameplay packed or
# candidate target is linked here.
target_link_libraries(rr_judgement_runtime PRIVATE Cuexis::Core cuexis::judgement)

# Two negative translation units, each built in its own configure run. They must fail because the
# header is not visible from this consumer's include roots:
#
#   - `author`:   the old author timebase header, which belongs to the compile side
#   - `timing`:   the shared timing header, which the judgement runtime does not provide
set(CUEXIS_JUDGEMENT_CONSUMER_NEGATIVE "" CACHE STRING
    "Negative translation unit to build: author or timing")
if(CUEXIS_JUDGEMENT_CONSUMER_NEGATIVE STREQUAL "author")
    add_executable(rr_judgement_runtime_negative_author negative_author_timebase.cpp)
    target_compile_features(rr_judgement_runtime_negative_author PRIVATE cxx_std_20)
    target_link_libraries(rr_judgement_runtime_negative_author PRIVATE Cuexis::Core cuexis::judgement)
elseif(CUEXIS_JUDGEMENT_CONSUMER_NEGATIVE STREQUAL "timing")
    add_executable(rr_judgement_runtime_negative_timing negative_timing_map.cpp)
    target_compile_features(rr_judgement_runtime_negative_timing PRIVATE cxx_std_20)
    target_link_libraries(rr_judgement_runtime_negative_timing PRIVATE Cuexis::Core cuexis::judgement)
elseif(NOT CUEXIS_JUDGEMENT_CONSUMER_NEGATIVE STREQUAL "")
    message(FATAL_ERROR "Unknown CUEXIS_JUDGEMENT_CONSUMER_NEGATIVE value: ${CUEXIS_JUDGEMENT_CONSUMER_NEGATIVE}")
endif()

enable_testing()
add_test(NAME rr_judgement_runtime COMMAND rr_judgement_runtime)
