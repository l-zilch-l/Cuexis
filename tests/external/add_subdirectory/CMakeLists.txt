cmake_minimum_required(VERSION 3.25)

project(CuexisExternalAddSubdirectory LANGUAGES CXX)

if(NOT DEFINED CUEXIS_SOURCE_DIR)
    message(FATAL_ERROR "CUEXIS_SOURCE_DIR is required")
endif()

set(CUEXIS_BUILD_PLAYER OFF CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_SDL_ADAPTER OFF CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_AUDIO_SDL_ADAPTER OFF CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_OPENGL_ADAPTER OFF CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_DEVELOPER_TOOLS OFF CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory("${CUEXIS_SOURCE_DIR}" cuexis)

get_target_property(cuexis_playback_type Cuexis::Playback TYPE)
if(CUEXIS_LIBRARY_TYPE STREQUAL "SHARED")
    set(expected_playback_type SHARED_LIBRARY)
else()
    set(expected_playback_type STATIC_LIBRARY)
endif()
if(NOT cuexis_playback_type STREQUAL expected_playback_type)
    message(FATAL_ERROR "Unexpected Cuexis::Playback type: ${cuexis_playback_type}")
endif()

add_executable(cuexis_external_consumer
    ../consumer.cpp
    ../../presentation/validation_sink.cpp
)
target_compile_definitions(cuexis_external_consumer
    PRIVATE CUEXIS_SOURCE_DIR="${CUEXIS_SOURCE_DIR}")
target_link_libraries(cuexis_external_consumer
    PRIVATE Cuexis::Playback Cuexis::Content Cuexis::Audio)

enable_testing()
add_test(NAME cuexis_external_consumer_run COMMAND cuexis_external_consumer)
