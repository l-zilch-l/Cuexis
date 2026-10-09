cmake_minimum_required(VERSION 3.25)
project(CuexisInstalledGameplayConsumer LANGUAGES CXX)

find_package(Cuexis 0.7.1 CONFIG REQUIRED COMPONENTS Playback)
add_executable(cuexis_gameplay_consumer ../gameplay_consumer.cpp)
target_link_libraries(cuexis_gameplay_consumer PRIVATE Cuexis::Playback)
target_compile_features(cuexis_gameplay_consumer PRIVATE cxx_std_20)
