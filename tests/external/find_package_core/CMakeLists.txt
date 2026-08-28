cmake_minimum_required(VERSION 3.25)

project(CuexisExternalFindPackageCore LANGUAGES CXX)

find_package(Cuexis 0.7 CONFIG REQUIRED COMPONENTS Core)

if(NOT Cuexis_Core_FOUND OR NOT TARGET Cuexis::Core OR
   NOT Cuexis_VERSION VERSION_EQUAL "0.7.0" OR
   NOT Cuexis_API_VERSION VERSION_EQUAL "0.7.0" OR
   NOT Cuexis_LIBRARY_TYPE STREQUAL CUEXIS_EXPECTED_LIBRARY_TYPE)
    message(FATAL_ERROR "Cuexis Core package component contract is invalid")
endif()

add_executable(cuexis_external_consumer ../core_consumer.cpp)
target_link_libraries(cuexis_external_consumer PRIVATE Cuexis::Core)

enable_testing()
add_test(NAME cuexis_external_consumer_run COMMAND cuexis_external_consumer)
