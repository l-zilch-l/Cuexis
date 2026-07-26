cmake_minimum_required(VERSION 3.25)

project(CuexisExternalFindPackage LANGUAGES CXX)

find_package(Cuexis CONFIG REQUIRED COMPONENTS Playback Content)

add_executable(cuexis_external_consumer ../consumer.cpp)
target_link_libraries(cuexis_external_consumer PRIVATE Cuexis::Playback Cuexis::Content)

enable_testing()
add_test(NAME cuexis_external_consumer_run COMMAND cuexis_external_consumer)
