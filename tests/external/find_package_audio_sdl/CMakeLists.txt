cmake_minimum_required(VERSION 3.25)

project(CuexisExternalFindPackageAudioSDL LANGUAGES CXX)

find_package(Cuexis 0.2 CONFIG REQUIRED COMPONENTS Audio AudioSDL)

if(NOT Cuexis_VERSION VERSION_EQUAL "0.2.0" OR
   NOT Cuexis_API_VERSION VERSION_EQUAL "0.2.0" OR
   NOT Cuexis_AudioSDL_FOUND)
    message(FATAL_ERROR "Cuexis AudioSDL package version or component contract is invalid")
endif()

add_executable(cuexis_external_consumer ../audio_sdl_consumer.cpp)
target_link_libraries(cuexis_external_consumer PRIVATE Cuexis::AudioSDL)

enable_testing()
add_test(NAME cuexis_external_consumer_run COMMAND cuexis_external_consumer)
