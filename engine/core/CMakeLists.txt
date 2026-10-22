find_package(tl-expected CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

add_library(cuexis_core ${CUEXIS_LIBRARY_TYPE})
add_library(cuexis::core ALIAS cuexis_core)
add_library(Cuexis::Core ALIAS cuexis_core)

generate_export_header(cuexis_core
    BASE_NAME CUEXIS_CORE
    EXPORT_MACRO_NAME CUEXIS_CORE_API
    EXPORT_FILE_NAME "${PROJECT_BINARY_DIR}/generated/cuexis/core/core_export.hpp"
)
if(CUEXIS_LIBRARY_TYPE STREQUAL "STATIC")
    target_compile_definitions(cuexis_core PUBLIC CUEXIS_CORE_STATIC_DEFINE)
else()
    target_compile_definitions(cuexis_core PRIVATE CUEXIS_CORE_EXPORTS)
endif()

set_target_properties(cuexis_core PROPERTIES EXPORT_NAME Core)
cuexis_configure_public_library(cuexis_core cuexis_core)

target_sources(cuexis_core
    PRIVATE
        src/error.cpp
        src/diagnostic.cpp
        src/log_sink.cpp
        src/math.cpp
        src/portable_path.cpp
        src/sha256.cpp
        src/thread_checker.cpp
        src/uuid.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include "${PROJECT_BINARY_DIR}/generated"
        FILES
            "${PROJECT_BINARY_DIR}/generated/cuexis/core/core_export.hpp"
            include/cuexis/core/abi_warnings.hpp
            include/cuexis/core/error.hpp
            include/cuexis/core/diagnostic.hpp
            include/cuexis/core/log_sink.hpp
            include/cuexis/core/math.hpp
            include/cuexis/core/result.hpp
            include/cuexis/core/thread_checker.hpp
            include/cuexis/core/uuid.hpp
)

target_compile_features(cuexis_core PUBLIC cxx_std_20)

target_include_directories(cuexis_core
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/generated>
        # In-tree only. Do not add internal/ to FILE_SET HEADERS.
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/internal>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(cuexis_core
    PUBLIC
        tl::expected
    PRIVATE
        glm::glm
)
