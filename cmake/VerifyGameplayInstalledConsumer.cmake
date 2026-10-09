cmake_minimum_required(VERSION 3.25)
foreach(required IN ITEMS CUEXIS_SOURCE_DIR CUEXIS_BINARY_DIR CUEXIS_PLAYBACK_TESTS
        CUEXIS_GENERATOR CUEXIS_BUILD_TYPE CUEXIS_CXX_COMPILER)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
function(run_checked description)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${description} failed (${status}):\n${output}\n${errors}")
    endif()
    message(STATUS "${description}: ${output}")
endfunction()
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef nonce)
set(work "${CUEXIS_BINARY_DIR}/s7a78-installed/${nonce}")
set(prefix "${work}/prefix")
file(MAKE_DIRECTORY "${work}")
run_checked("Owning typed fixture" "${CUEXIS_PLAYBACK_TESTS}" "[.candidate-gameplay-fixture]")
run_checked("Clean experimental SDK installation" "${CMAKE_COMMAND}" --install "${CUEXIS_BINARY_DIR}"
    --prefix "${prefix}" --config "${CUEXIS_BUILD_TYPE}")
if(NOT EXISTS "${prefix}/include/cuexis/playback/gameplay_candidate.hpp")
    message(FATAL_ERROR "Candidate public header is missing")
endif()
foreach(private IN ITEMS judgement gameplay_packed runtime world)
    if(EXISTS "${prefix}/include/cuexis/${private}")
        message(FATAL_ERROR "Candidate SDK installed internal ${private} execution headers")
    endif()
endforeach()
set(common -G "${CUEXIS_GENERATOR}" "-DCMAKE_BUILD_TYPE=${CUEXIS_BUILD_TYPE}"
    "-DCMAKE_CXX_COMPILER=${CUEXIS_CXX_COMPILER}"
    "-DCuexis_DIR=${prefix}/lib/cmake/Cuexis"
    "-DCMAKE_PREFIX_PATH=${CUEXIS_VCPKG_INSTALLED_DIR}/${CUEXIS_VCPKG_TARGET_TRIPLET}")
if(CUEXIS_ENABLE_SANITIZERS)
    list(APPEND common
        "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer"
        "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer")
endif()
foreach(optional IN ITEMS CMAKE_MAKE_PROGRAM CMAKE_RC_COMPILER CMAKE_MT)
    if(DEFINED ${optional} AND NOT "${${optional}}" STREQUAL "")
        list(APPEND common "-D${optional}=${${optional}}")
    endif()
endforeach()
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${CUEXIS_SOURCE_DIR}/tests/external/find_package_gameplay"
    -B "${work}/default-off" ${common} RESULT_VARIABLE rejected OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(rejected EQUAL 0 OR NOT "${output}${errors}" MATCHES "experimental")
    message(FATAL_ERROR "The installed candidate SDK was not rejected without explicit opt-in: ${output}${errors}")
endif()
run_checked("Installed consumer configure" "${CMAKE_COMMAND}"
    -S "${CUEXIS_SOURCE_DIR}/tests/external/find_package_gameplay" -B "${work}/consumer"
    ${common} -DCuexis_ALLOW_EXPERIMENTAL=ON)
run_checked("Installed consumer build" "${CMAKE_COMMAND}" --build "${work}/consumer" --config "${CUEXIS_BUILD_TYPE}")
set(program "${work}/consumer/cuexis_gameplay_consumer${CMAKE_EXECUTABLE_SUFFIX}")
if(WIN32)
    set(program "${work}/consumer/cuexis_gameplay_consumer.exe")
    set(ENV{PATH} "${prefix}/bin;${CUEXIS_VCPKG_INSTALLED_DIR}/${CUEXIS_VCPKG_TARGET_TRIPLET}/debug/bin;${CUEXIS_VCPKG_INSTALLED_DIR}/${CUEXIS_VCPKG_TARGET_TRIPLET}/bin;$ENV{PATH}")
elseif(UNIX AND NOT APPLE)
    set(ENV{LD_LIBRARY_PATH} "${prefix}/lib")
endif()
run_checked("Actual installed GameplayOnly lifecycle" "${program}" "${CUEXIS_BINARY_DIR}/s7a78-public-fixture/main.packed")
message(STATUS "S7A78 installed Gameplay-only consumer passed; work=${work}")

if(DEFINED CUEXIS_GAMEPLAY_TOOL AND NOT "${CUEXIS_GAMEPLAY_TOOL}" STREQUAL "")
    run_checked("Installed reference host configure" "${CMAKE_COMMAND}"
        -S "${CUEXIS_SOURCE_DIR}/examples/reference_host" -B "${work}/host"
        ${common} -DCuexis_ALLOW_EXPERIMENTAL=ON)
    run_checked("Installed reference host build" "${CMAKE_COMMAND}" --build "${work}/host")
    set(host "${work}/host/cuexis_reference_host")
    if(WIN32)
        string(APPEND host ".exe")
    endif()
    find_program(gameplay_python NAMES python3 python REQUIRED)
    run_checked("Installed reference host Gameplay matrix" "${gameplay_python}" -B
        "${CUEXIS_SOURCE_DIR}/tools/check_gameplay_host_tests.py"
        --host "${host}" --tool "${CUEXIS_GAMEPLAY_TOOL}"
        --fixture "${CUEXIS_BINARY_DIR}/s7a78-public-fixture" --log-dir "${work}/host-logs")
endif()
