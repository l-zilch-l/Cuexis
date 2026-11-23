cmake_minimum_required(VERSION 3.25)
project(CuexisExperimentalHeaderNegative LANGUAGES CXX)

# Negative installed-SDK probe: the default package must not expose the experimental Gameplay
# headers, so this target must fail to compile. It is never linked or run.
find_package(Cuexis 0.9.1 CONFIG REQUIRED COMPONENTS Playback)
add_library(cuexis_experimental_header_negative OBJECT experimental_header_negative.cpp)
target_link_libraries(cuexis_experimental_header_negative PRIVATE Cuexis::Playback)
target_compile_features(cuexis_experimental_header_negative PRIVATE cxx_std_20)
