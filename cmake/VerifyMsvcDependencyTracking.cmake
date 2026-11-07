foreach(required CUEXIS_SOURCE_DIR CUEXIS_VERIFY_DIR CUEXIS_VERIFY_COMPILER CUEXIS_VERIFY_NINJA)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing ${required}")
    endif()
endforeach()

# Retain each invocation and its logs. Never overwrite a prior verification build.
file(TO_CMAKE_PATH "${CUEXIS_SOURCE_DIR}" source_root)
foreach(layout standalone embedded)
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef invocation)
set(work "${CUEXIS_VERIFY_DIR}/msvc-includes-${layout}-${invocation}")
file(MAKE_DIRECTORY "${work}/source")
if(layout STREQUAL "standalone")
    set(load_module "include(\"${source_root}/cmake/CuexisMsvcDependencies.cmake\")\n")
else()
    file(MAKE_DIRECTORY "${work}/source/embedded")
    file(WRITE "${work}/source/embedded/CMakeLists.txt"
        "project(EmbeddedCuexis LANGUAGES CXX)\n"
        "include(\"${source_root}/cmake/CuexisMsvcDependencies.cmake\")\n")
    set(load_module "add_subdirectory(embedded)\n")
endif()
file(WRITE "${work}/source/CMakeLists.txt"
    "cmake_minimum_required(VERSION 3.25)\n"
    "project(CuexisIncludeCanary LANGUAGES CXX)\n"
    "${load_module}"
    "add_executable(canary main.cpp)\n")
file(WRITE "${work}/source/value.hpp" "#pragma once\n#define CANARY_VALUE 17\n")
file(WRITE "${work}/source/main.cpp"
    "#include \"value.hpp\"\nint main() { return CANARY_VALUE; }\n")

execute_process(COMMAND "${CMAKE_COMMAND}" -S "${work}/source" -B "${work}/build" -G Ninja
    "-DCMAKE_CXX_COMPILER=${CUEXIS_VERIFY_COMPILER}"
    "-DCMAKE_CXX_COMPILER_ARG1=${CUEXIS_VERIFY_COMPILER_ARG1}"
    "-DCMAKE_MAKE_PROGRAM=${CUEXIS_VERIFY_NINJA}" -DCMAKE_BUILD_TYPE=Release
    RESULT_VARIABLE result OUTPUT_FILE "${work}/configure.log"
    ERROR_FILE "${work}/configure.err")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Dependency canary configure failed; see ${work}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${work}/build" --parallel 1
    RESULT_VARIABLE result OUTPUT_FILE "${work}/initial.log"
    ERROR_FILE "${work}/initial.err")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Dependency canary initial build failed; see ${work}")
endif()
execute_process(COMMAND "${work}/build/canary.exe" RESULT_VARIABLE initial)
execute_process(COMMAND "${CUEXIS_VERIFY_NINJA}" -C "${work}/build" -t deps
    RESULT_VARIABLE result OUTPUT_VARIABLE dependencies ERROR_VARIABLE dependency_error)
file(WRITE "${work}/dependencies.log" "${dependencies}")
if(NOT result EQUAL 0 OR NOT dependencies MATCHES "value\\.hpp")
    message(FATAL_ERROR "Ninja did not record the header dependency; see ${work}")
endif()

file(WRITE "${work}/source/value.hpp" "#pragma once\n#define CANARY_VALUE 18\n")
execute_process(COMMAND "${CUEXIS_VERIFY_NINJA}" -C "${work}/build" -d explain
    RESULT_VARIABLE result OUTPUT_FILE "${work}/rebuild.log"
    ERROR_FILE "${work}/rebuild.err")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Dependency canary header rebuild failed; see ${work}")
endif()
execute_process(COMMAND "${work}/build/canary.exe" RESULT_VARIABLE changed)
if(NOT initial EQUAL 17 OR NOT changed EQUAL 18)
    message(FATAL_ERROR
        "Header edit did not rebuild the executable: ${initial} -> ${changed}; see ${work}")
endif()
message(STATUS "MSVC ${layout} header dependency tracking passed: ${work}")
endforeach()
