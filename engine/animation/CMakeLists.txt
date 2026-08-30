add_library(cuexis_animation STATIC
    src/animation_compiler.cpp
    src/animation_mixer.cpp
    src/animation_program.cpp
    src/animation_sampler.cpp
    src/animation_system.cpp
)
add_library(cuexis::animation ALIAS cuexis_animation)

set_target_properties(cuexis_animation PROPERTIES EXPORT_NAME InternalAnimation)

target_sources(cuexis_animation
    PRIVATE
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/cuexis/animation/animation_compiler.hpp
            include/cuexis/animation/animation_diagnostics.hpp
            include/cuexis/animation/animation_mixer.hpp
            include/cuexis/animation/animation_program.hpp
            include/cuexis/animation/animation_sample.hpp
            include/cuexis/animation/animation_system.hpp
)

target_include_directories(cuexis_animation PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(cuexis_animation PUBLIC cxx_std_20)

target_link_libraries(cuexis_animation
    PUBLIC
        cuexis::core
        cuexis::chart
        cuexis::world
)
