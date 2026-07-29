set(CUEXIS_LIBRARY_TYPE "STATIC" CACHE STRING
    "Cuexis public library linkage (STATIC or SHARED)")
set_property(CACHE CUEXIS_LIBRARY_TYPE PROPERTY STRINGS STATIC SHARED)
if(NOT CUEXIS_LIBRARY_TYPE STREQUAL "STATIC" AND NOT CUEXIS_LIBRARY_TYPE STREQUAL "SHARED")
    message(FATAL_ERROR "CUEXIS_LIBRARY_TYPE must be STATIC or SHARED")
endif()

if(CUEXIS_LIBRARY_TYPE STREQUAL "SHARED")
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
endif()

if(NOT CUEXIS_SDK_API_VERSION MATCHES "^([0-9]+)[.]([0-9]+)[.]([0-9]+)$")
    message(FATAL_ERROR
        "CUEXIS_SDK_API_VERSION must contain major.minor.patch numeric components")
endif()
set(CUEXIS_SDK_API_MAJOR "${CMAKE_MATCH_1}")
set(CUEXIS_SDK_API_MINOR "${CMAKE_MATCH_2}")

function(cuexis_configure_public_library target output_name)
    if(CUEXIS_LIBRARY_TYPE STREQUAL "SHARED")
        set_target_properties(${target} PROPERTIES
            OUTPUT_NAME "${output_name}-${CUEXIS_SDK_API_MAJOR}.${CUEXIS_SDK_API_MINOR}"
            DEBUG_POSTFIX "d"
            VERSION "${CUEXIS_SDK_API_VERSION}"
            SOVERSION "${CUEXIS_SDK_API_MAJOR}.${CUEXIS_SDK_API_MINOR}"
        )
    endif()
endfunction()
