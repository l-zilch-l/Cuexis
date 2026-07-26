cmake_minimum_required(VERSION 3.25)

project(CuexisExternalFindPackage LANGUAGES CXX)

find_package(Cuexis 0.1 CONFIG REQUIRED COMPONENTS Playback Content)

if(NOT Cuexis_VERSION VERSION_EQUAL "0.1.0" OR
   NOT Cuexis_API_VERSION VERSION_EQUAL "0.1.0" OR
   Cuexis_VERSION_DISPLAY STREQUAL "")
    message(FATAL_ERROR
        "Cuexis package version contract is invalid: API='${Cuexis_API_VERSION}', "
        "VERSION='${Cuexis_VERSION}', DISPLAY='${Cuexis_VERSION_DISPLAY}'"
    )
endif()

add_executable(cuexis_external_consumer ../consumer.cpp)
target_link_libraries(cuexis_external_consumer PRIVATE Cuexis::Playback Cuexis::Content)

enable_testing()
add_test(NAME cuexis_external_consumer_run COMMAND cuexis_external_consumer)
