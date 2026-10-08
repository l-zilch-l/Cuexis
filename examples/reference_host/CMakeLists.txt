cmake_minimum_required(VERSION 3.25)

# Cuexis Reference Host: a standalone host project that consumes the installed
# Cuexis Playback SDK through find_package(). It never reads the Cuexis source
# tree, never links an internal target, and never links the Player
# configuration implementation.
#
# ADR 0042 S6-D08 freezes this example at examples/reference_host/ and freezes
# SDK API 0.7.1 as the baseline this host is written against.

project(CuexisReferenceHost LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED CUEXIS_HOST_API_VERSION OR CUEXIS_HOST_API_VERSION STREQUAL "")
    # The baseline the host was written against. An incompatible SDK minor must
    # be refused at configure time rather than fail at run time.
    set(CUEXIS_HOST_API_VERSION 0.7.1)
endif()

find_package(Cuexis ${CUEXIS_HOST_API_VERSION} CONFIG REQUIRED COMPONENTS Playback)

if(NOT TARGET Cuexis::Playback)
    message(FATAL_ERROR "Cuexis::Playback is required by the reference host")
endif()
if(Cuexis_API_VERSION VERSION_LESS "0.7.1" OR NOT Cuexis_API_VERSION VERSION_LESS "0.8.0")
    message(FATAL_ERROR
        "The reference host supports SDK API 0.7.x; found ${Cuexis_API_VERSION}")
endif()

add_executable(cuexis_reference_host
    src/main.cpp
    src/host_clock.cpp
    src/host_commands.cpp
    src/host_content.cpp
    src/host_report.cpp
    src/host_runner.cpp
    src/host_gameplay.cpp)
target_include_directories(cuexis_reference_host PRIVATE src)
target_link_libraries(cuexis_reference_host PRIVATE Cuexis::Playback)
target_compile_definitions(cuexis_reference_host PRIVATE
    CUEXIS_HOST_BUILD_FLAVOR="${Cuexis_BUILD_FLAVOR}")

if(MSVC)
    target_compile_options(cuexis_reference_host PRIVATE /W4 /utf-8)
else()
    target_compile_options(cuexis_reference_host PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(DEFINED CUEXIS_HOST_CONTENT_DIR AND NOT CUEXIS_HOST_CONTENT_DIR STREQUAL "")
    enable_testing()
    add_test(NAME cuexis_reference_host_run COMMAND cuexis_reference_host
        --content "${CUEXIS_HOST_CONTENT_DIR}")
endif()
