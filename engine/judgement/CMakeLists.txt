add_library(cuexis_judgement STATIC)
add_library(cuexis::judgement ALIAS cuexis_judgement)

set_target_properties(cuexis_judgement PROPERTIES EXPORT_NAME InternalJudgement)

target_sources(cuexis_judgement
    PRIVATE
        src/diagnostic.cpp
        src/execution_profile.cpp
        src/execution_kernel.cpp
        src/gameplay_assembler.cpp
        src/gameplay_graph.cpp
        src/gameplay_prepare.cpp
        src/input.cpp
        src/kernel_types.cpp
        src/session.cpp
        src/timebase.cpp
)

# S1-04 / S7A-1: the internal archive is exported through CUEXIS_STATIC_IMPLEMENTATION_TARGETS and
# the headers are not. There is deliberately no FILE_SET HEADERS and no install() rule here, so no
# judgement header can reach the install tree.
target_include_directories(cuexis_judgement PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(cuexis_judgement PUBLIC cxx_std_20)

# The typed kernel may depend on cuexis::core and on nothing else: no SDL, no OpenGL, no audio, no
# audio_sdl, no world, no EnTT, no JSON DOM, no runtime and no platform adapter.
target_link_libraries(cuexis_judgement
    PUBLIC
        cuexis::core
)
