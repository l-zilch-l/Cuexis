find_package(tl-expected CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

add_library(cuexis_core STATIC)
add_library(cuexis::core ALIAS cuexis_core)

target_sources(cuexis_core
    PRIVATE
        src/error.cpp
        src/diagnostic.cpp
        src/log.cpp
        src/math.cpp
        src/thread_checker.cpp
        src/uuid.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/cuexis/core/error.hpp
            include/cuexis/core/diagnostic.hpp
            include/cuexis/core/log.hpp
            include/cuexis/core/math.hpp
            include/cuexis/core/result.hpp
            include/cuexis/core/thread_checker.hpp
            include/cuexis/core/uuid.hpp
)

target_compile_features(cuexis_core PUBLIC cxx_std_20)

target_include_directories(cuexis_core
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/generated>
)

target_link_libraries(cuexis_core
    PUBLIC
        tl::expected
    PRIVATE
        fmt::fmt
        glm::glm
        spdlog::spdlog
)
