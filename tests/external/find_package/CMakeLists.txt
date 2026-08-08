cmake_minimum_required(VERSION 3.25)

project(CuexisExternalFindPackage LANGUAGES CXX)

if(NOT DEFINED CUEXIS_SOURCE_DIR)
    message(FATAL_ERROR "CUEXIS_SOURCE_DIR is required")
endif()

find_package(Cuexis 0.5 CONFIG REQUIRED COMPONENTS Playback Content Audio)

if(NOT Cuexis_VERSION VERSION_EQUAL "0.5.0" OR
   NOT Cuexis_API_VERSION VERSION_EQUAL "0.5.0" OR
   NOT Cuexis_LIBRARY_TYPE STREQUAL CUEXIS_EXPECTED_LIBRARY_TYPE OR
   NOT Cuexis_COMPILER_ID STREQUAL CMAKE_CXX_COMPILER_ID OR
   NOT Cuexis_COMPILER_VERSION STREQUAL CMAKE_CXX_COMPILER_VERSION OR
   NOT Cuexis_POINTER_SIZE STREQUAL CMAKE_SIZEOF_VOID_P OR
   NOT Cuexis_CXX_STANDARD STREQUAL "20" OR
   NOT Cuexis_SYSTEM_NAME STREQUAL CMAKE_SYSTEM_NAME OR
   Cuexis_VERSION_DISPLAY STREQUAL "")
    message(FATAL_ERROR
        "Cuexis package version contract is invalid: API='${Cuexis_API_VERSION}', "
        "VERSION='${Cuexis_VERSION}', DISPLAY='${Cuexis_VERSION_DISPLAY}'"
    )
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
