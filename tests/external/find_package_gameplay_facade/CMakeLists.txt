cmake_minimum_required(VERSION 3.25)
project(CuexisInstalledGameplayFacadeConsumer LANGUAGES CXX)

find_package(Cuexis 0.10.0 CONFIG REQUIRED COMPONENTS Playback)
add_executable(cuexis_gameplay_facade_consumer ../gameplay_facade_consumer.cpp)
target_link_libraries(cuexis_gameplay_facade_consumer PRIVATE Cuexis::Playback)
target_compile_features(cuexis_gameplay_facade_consumer PRIVATE cxx_std_20)
