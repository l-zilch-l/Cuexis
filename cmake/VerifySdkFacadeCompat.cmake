cmake_minimum_required(VERSION 3.25)
# RR-4 installed-SDK facade compatibility gate. It exports the two candidate fixtures, installs the
# experimental package, builds an independent installed consumer that drives the old members and the
# new experimental facade in two separate sessions, and requires the same identity, the same
# Paused/Resume transition and a rejected tampered reconciliation that keeps the session Paused.
foreach(required IN ITEMS CUEXIS_SOURCE_DIR CUEXIS_BINARY_DIR CUEXIS_PLAYBACK_TESTS
        CUEXIS_GENERATOR CUEXIS_BUILD_TYPE CUEXIS_CXX_COMPILER)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
function(run_checked description)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE status OUTPUT_VARIABLE output
                    ERROR_VARIABLE errors)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${description} failed (${status}):\n${output}\n${errors}")
    endif()
    message(STATUS "${description}: ${output}")
endfunction()

string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef nonce)
set(work "${CUEXIS_BINARY_DIR}/s7t08-facade/${nonce}")
set(prefix "${work}/prefix")
file(MAKE_DIRECTORY "${work}")

# The compiled fixture writes main.packed plus configuration.json into the build-tree directory the
# Playback test target was configured with.
run_checked("Owning typed fixture" "${CUEXIS_PLAYBACK_TESTS}" "[.candidate-gameplay-fixture]")
set(fixture "${CUEXIS_BINARY_DIR}/s7a78-public-fixture")
foreach(required_fixture IN ITEMS main.packed configuration.json)
    if(NOT EXISTS "${fixture}/${required_fixture}")
        message(FATAL_ERROR "Candidate fixture is missing ${required_fixture} under ${fixture}")
    endif()
endforeach()
set(realtime "${work}/realtime.packed")
run_checked("Realtime compiled fixture" "${CMAKE_COMMAND}" -E env
    "CUEXIS_B3_EXPORT_FILE=${realtime}"
    "${CUEXIS_PLAYBACK_TESTS}" "Candidate realtime execution uses explicit profile and coherent clock")

run_checked("Experimental SDK installation" "${CMAKE_COMMAND}" --install "${CUEXIS_BINARY_DIR}"
    --prefix "${prefix}" --config "${CUEXIS_BUILD_TYPE}")
foreach(experimental_header IN ITEMS
        include/cuexis/playback/experimental/gameplay.hpp
        include/cuexis/playback/experimental/gameplay_realtime.hpp
        include/cuexis/playback/experimental/gameplay_replay.hpp)
    if(NOT EXISTS "${prefix}/${experimental_header}")
        message(FATAL_ERROR "Experimental SDK is missing ${experimental_header}")
    endif()
endforeach()
foreach(internal_header IN ITEMS
        include/cuexis/playback/execution_host_access.hpp
        include/cuexis/playback/replay_identity.hpp
        include/cuexis/playback/frame_layout_access.hpp
        include/cuexis/playback/internal)
    if(EXISTS "${prefix}/${internal_header}")
        message(FATAL_ERROR
            "Experimental SDK installed the build-only internal entry ${internal_header}")
    endif()
endforeach()

set(common -G "${CUEXIS_GENERATOR}" "-DCMAKE_BUILD_TYPE=${CUEXIS_BUILD_TYPE}"
    "-DCMAKE_CXX_COMPILER=${CUEXIS_CXX_COMPILER}"
    "-DCuexis_DIR=${prefix}/lib/cmake/Cuexis"
    "-DCMAKE_PREFIX_PATH=${CUEXIS_VCPKG_INSTALLED_DIR}/${CUEXIS_VCPKG_TARGET_TRIPLET}")
foreach(optional IN ITEMS CMAKE_MAKE_PROGRAM CMAKE_RC_COMPILER CMAKE_MT)
    if(DEFINED ${optional} AND NOT "${${optional}}" STREQUAL "")
        list(APPEND common "-D${optional}=${${optional}}")
    endif()
endforeach()

execute_process(COMMAND "${CMAKE_COMMAND}" -S
    "${CUEXIS_SOURCE_DIR}/tests/external/find_package_gameplay_facade"
    -B "${work}/default-off" ${common} RESULT_VARIABLE rejected OUTPUT_VARIABLE output
    ERROR_VARIABLE errors)
if(rejected EQUAL 0 OR NOT "${output}${errors}" MATCHES "experimental")
    message(FATAL_ERROR
        "The installed candidate facade package was not rejected without explicit opt-in: "
        "${output}${errors}")
endif()
run_checked("Facade consumer configure" "${CMAKE_COMMAND}" -S
    "${CUEXIS_SOURCE_DIR}/tests/external/find_package_gameplay_facade" -B "${work}/consumer"
    ${common} -DCuexis_ALLOW_EXPERIMENTAL=ON)
run_checked("Facade consumer build" "${CMAKE_COMMAND}" --build "${work}/consumer"
    --config "${CUEXIS_BUILD_TYPE}")
set(program "${work}/consumer/cuexis_gameplay_facade_consumer${CMAKE_EXECUTABLE_SUFFIX}")
if(WIN32)
    set(program "${work}/consumer/cuexis_gameplay_facade_consumer.exe")
    set(ENV{PATH} "${prefix}/bin;${CUEXIS_VCPKG_INSTALLED_DIR}/${CUEXIS_VCPKG_TARGET_TRIPLET}/debug/bin;${CUEXIS_VCPKG_INSTALLED_DIR}/${CUEXIS_VCPKG_TARGET_TRIPLET}/bin;$ENV{PATH}")
elseif(UNIX AND NOT APPLE)
    set(ENV{LD_LIBRARY_PATH} "${prefix}/lib")
endif()
run_checked("Installed facade Resume parity" "${program}" "${fixture}/main.packed" "${realtime}"
    "${fixture}/configuration.json")
message(STATUS "S7T08 experimental SDK facade compatibility passed; work=${work}")
