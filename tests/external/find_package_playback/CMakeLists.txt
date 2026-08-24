cmake_minimum_required(VERSION 3.25)

project(CuexisExternalFindPackagePlayback LANGUAGES CXX)

if(NOT DEFINED CUEXIS_SOURCE_DIR)
    message(FATAL_ERROR "CUEXIS_SOURCE_DIR is required")
endif()
if(NOT DEFINED CUEXIS_FIXTURE_DIR)
    message(FATAL_ERROR "CUEXIS_FIXTURE_DIR is required")
endif()

if(NOT DEFINED CUEXIS_FIND_VERSION OR CUEXIS_FIND_VERSION STREQUAL "")
    set(CUEXIS_FIND_VERSION 0.6)
endif()
if(NOT DEFINED CUEXIS_COMPONENTS OR CUEXIS_COMPONENTS STREQUAL "")
    set(CUEXIS_COMPONENTS Playback)
endif()

find_package(Cuexis ${CUEXIS_FIND_VERSION} CONFIG REQUIRED COMPONENTS ${CUEXIS_COMPONENTS})

if(NOT Cuexis_Playback_FOUND OR NOT TARGET Cuexis::Playback OR
   NOT Cuexis_VERSION VERSION_EQUAL "0.6.0" OR
   NOT Cuexis_API_VERSION VERSION_EQUAL "0.6.0" OR
   NOT Cuexis_LIBRARY_TYPE STREQUAL CUEXIS_EXPECTED_LIBRARY_TYPE OR
   NOT Cuexis_COMPILER_ID STREQUAL CMAKE_CXX_COMPILER_ID OR
   NOT Cuexis_COMPILER_VERSION STREQUAL CMAKE_CXX_COMPILER_VERSION OR
   NOT Cuexis_POINTER_SIZE STREQUAL CMAKE_SIZEOF_VOID_P OR
   NOT Cuexis_CXX_STANDARD STREQUAL "20")
    message(FATAL_ERROR "Cuexis Playback-only package contract is invalid")
endif()

add_executable(cuexis_external_consumer ../playback_consumer.cpp)
target_compile_definitions(cuexis_external_consumer
    PRIVATE CUEXIS_FIXTURE_DIR="${CUEXIS_FIXTURE_DIR}")
target_link_libraries(cuexis_external_consumer PRIVATE Cuexis::Playback)

enable_testing()
add_test(NAME cuexis_external_consumer_run COMMAND cuexis_external_consumer)
