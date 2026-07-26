find_package(tl-expected CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

add_library(cuexis_core STATIC)
add_library(cuexis::core ALIAS cuexis_core)
add_library(Cuexis::Core ALIAS cuexis_core)

set_target_properties(cuexis_core PROPERTIES EXPORT_NAME Core)

target_sources(cuexis_core
    PRIVATE
        src/error.cpp
        src/diagnostic.cpp
        src/log_sink.cpp
        src/math.cpp
        src/thread_checker.cpp
        src/uuid.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
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
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(cuexis_core
    PUBLIC
        tl::expected
    PRIVATE
        glm::glm
)
