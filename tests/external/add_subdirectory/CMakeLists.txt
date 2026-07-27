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

add_executable(cuexis_external_consumer ../consumer.cpp)
target_link_libraries(cuexis_external_consumer
    PRIVATE Cuexis::Playback Cuexis::Content Cuexis::Audio)

enable_testing()
add_test(NAME cuexis_external_consumer_run COMMAND cuexis_external_consumer)
