set(CUEXIS_VERSION_YEAR 26)
set(CUEXIS_VERSION_MONTH 7)
set(CUEXIS_VERSION_DAY 18)
set(CUEXIS_VERSION_HOUR 18)
set(CUEXIS_VERSION_BUILD 1)

# Source-compatibility version of the installable C++ Playback preview.
# This is intentionally independent from the date-based build identity below.
set(CUEXIS_SDK_API_VERSION "0.4.0")

set(CUEXIS_VERSION_SUFFIX "" CACHE STRING "Optional Cuexis build suffix")

function(cuexis_validate_component name value minimum maximum)
    if(NOT "${value}" MATCHES "^[0-9]+$")
        message(FATAL_ERROR "${name} must be an unsigned integer, got '${value}'")
    endif()
    if(value LESS minimum OR value GREATER maximum)
        message(FATAL_ERROR "${name} must be in [${minimum}, ${maximum}], got '${value}'")
    endif()
endfunction()

function(cuexis_pad_two value output)
    if(value LESS 10)
        set(${output} "0${value}" PARENT_SCOPE)
    else()
        set(${output} "${value}" PARENT_SCOPE)
    endif()
endfunction()

cuexis_validate_component("CUEXIS_VERSION_YEAR" "${CUEXIS_VERSION_YEAR}" 0 99)
cuexis_validate_component("CUEXIS_VERSION_MONTH" "${CUEXIS_VERSION_MONTH}" 1 12)
cuexis_validate_component("CUEXIS_VERSION_DAY" "${CUEXIS_VERSION_DAY}" 1 31)
cuexis_validate_component("CUEXIS_VERSION_HOUR" "${CUEXIS_VERSION_HOUR}" 0 23)
cuexis_validate_component("CUEXIS_VERSION_BUILD" "${CUEXIS_VERSION_BUILD}" 1 2147483647)

set(cuexis_days_per_month 31 28 31 30 31 30 31 31 30 31 30 31)
math(EXPR cuexis_full_year "2000 + ${CUEXIS_VERSION_YEAR}")
math(EXPR cuexis_divisible_by_4 "${cuexis_full_year} % 4")
math(EXPR cuexis_divisible_by_100 "${cuexis_full_year} % 100")
math(EXPR cuexis_divisible_by_400 "${cuexis_full_year} % 400")
if((cuexis_divisible_by_4 EQUAL 0 AND NOT cuexis_divisible_by_100 EQUAL 0)
    OR cuexis_divisible_by_400 EQUAL 0)
    list(REMOVE_AT cuexis_days_per_month 1)
    list(INSERT cuexis_days_per_month 1 29)
endif()
math(EXPR cuexis_month_index "${CUEXIS_VERSION_MONTH} - 1")
list(GET cuexis_days_per_month ${cuexis_month_index} cuexis_max_day)
if(CUEXIS_VERSION_DAY GREATER cuexis_max_day)
    message(FATAL_ERROR
        "CUEXIS_VERSION_DAY ${CUEXIS_VERSION_DAY} is invalid for month ${CUEXIS_VERSION_MONTH}"
    )
endif()

if(NOT CUEXIS_VERSION_SUFFIX MATCHES "^$|^(dev|test|internal)$|^exp[.][A-Za-z0-9][A-Za-z0-9._-]*$")
    message(FATAL_ERROR "Invalid CUEXIS_VERSION_SUFFIX '${CUEXIS_VERSION_SUFFIX}'")
endif()

cuexis_pad_two("${CUEXIS_VERSION_YEAR}" CUEXIS_VERSION_YEAR_PADDED)
cuexis_pad_two("${CUEXIS_VERSION_MONTH}" CUEXIS_VERSION_MONTH_PADDED)
cuexis_pad_two("${CUEXIS_VERSION_DAY}" CUEXIS_VERSION_DAY_PADDED)
cuexis_pad_two("${CUEXIS_VERSION_HOUR}" CUEXIS_VERSION_HOUR_PADDED)

set(CUEXIS_CMAKE_VERSION
    "${CUEXIS_VERSION_YEAR}.${CUEXIS_VERSION_MONTH}.${CUEXIS_VERSION_DAY}.${CUEXIS_VERSION_HOUR}"
)
set(CUEXIS_VERSION_CANONICAL
    "${CUEXIS_VERSION_YEAR_PADDED}.${CUEXIS_VERSION_MONTH_PADDED}.${CUEXIS_VERSION_DAY_PADDED}.${CUEXIS_VERSION_HOUR_PADDED}-${CUEXIS_VERSION_BUILD}"
)

if(CUEXIS_VERSION_SUFFIX STREQUAL "")
    set(CUEXIS_VERSION_DISPLAY "${CUEXIS_VERSION_CANONICAL}")
else()
    set(CUEXIS_VERSION_DISPLAY "${CUEXIS_VERSION_CANONICAL}-${CUEXIS_VERSION_SUFFIX}")
endif()

file(READ "${CMAKE_CURRENT_LIST_DIR}/../vcpkg.json" CUEXIS_VCPKG_MANIFEST_JSON)
string(JSON CUEXIS_VCPKG_VERSION ERROR_VARIABLE CUEXIS_VCPKG_VERSION_ERROR
    GET "${CUEXIS_VCPKG_MANIFEST_JSON}" version-string
)
if(CUEXIS_VCPKG_VERSION_ERROR)
    message(FATAL_ERROR "Cannot read vcpkg.json version-string: ${CUEXIS_VCPKG_VERSION_ERROR}")
endif()
if(NOT CUEXIS_VCPKG_VERSION STREQUAL CUEXIS_VERSION_CANONICAL)
    message(FATAL_ERROR
        "Version mismatch: CuexisVersion.cmake is '${CUEXIS_VERSION_CANONICAL}', "
        "vcpkg.json is '${CUEXIS_VCPKG_VERSION}'"
    )
endif()
