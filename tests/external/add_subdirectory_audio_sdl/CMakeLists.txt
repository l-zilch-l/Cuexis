cmake_minimum_required(VERSION 3.25)

project(CuexisExternalAddSubdirectoryAudioSDL LANGUAGES CXX)

if(NOT DEFINED CUEXIS_SOURCE_DIR)
    message(FATAL_ERROR "CUEXIS_SOURCE_DIR is required")
endif()

set(CUEXIS_BUILD_PLAYER OFF CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_SDL_ADAPTER OFF CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_AUDIO_SDL_ADAPTER ON CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_OPENGL_ADAPTER OFF CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_DEVELOPER_TOOLS OFF CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory("${CUEXIS_SOURCE_DIR}" cuexis)

get_target_property(cuexis_audio_sdl_type Cuexis::AudioSDL TYPE)
if(CUEXIS_LIBRARY_TYPE STREQUAL "SHARED")
    set(expected_audio_sdl_type SHARED_LIBRARY)
else()
    set(expected_audio_sdl_type STATIC_LIBRARY)
endif()
if(NOT cuexis_audio_sdl_type STREQUAL expected_audio_sdl_type)
    message(FATAL_ERROR "Unexpected Cuexis::AudioSDL type: ${cuexis_audio_sdl_type}")
endif()

add_executable(cuexis_external_consumer ../audio_sdl_consumer.cpp)
target_link_libraries(cuexis_external_consumer PRIVATE Cuexis::AudioSDL)

enable_testing()
add_test(NAME cuexis_external_consumer_run COMMAND cuexis_external_consumer)
