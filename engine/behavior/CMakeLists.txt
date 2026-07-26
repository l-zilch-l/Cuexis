add_library(cuexis_behavior STATIC
    src/behavior_system.cpp
)
add_library(cuexis::behavior ALIAS cuexis_behavior)

set_target_properties(cuexis_behavior PROPERTIES EXPORT_NAME InternalBehavior)

target_sources(cuexis_behavior
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/cuexis/behavior/behavior_component.hpp
            include/cuexis/behavior/behavior_program.hpp
            include/cuexis/behavior/behavior_system.hpp
)

target_compile_features(cuexis_behavior PUBLIC cxx_std_20)

target_link_libraries(cuexis_behavior
    PUBLIC
        cuexis::core
        cuexis::world
)
