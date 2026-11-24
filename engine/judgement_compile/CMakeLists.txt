add_library(cuexis_judgement_compile STATIC)
add_library(cuexis::judgement_compile ALIAS cuexis_judgement_compile)
set_target_properties(cuexis_judgement_compile PROPERTIES EXPORT_NAME InternalJudgementCompile)

target_sources(cuexis_judgement_compile
    PRIVATE
        src/legacy_prepared_view.cpp
        src/legacy_runtime_identity.cpp
)

target_include_directories(cuexis_judgement_compile
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(cuexis_judgement_compile PUBLIC cxx_std_20)

# The compile side links the running judgement library; the runtime never links back.
target_link_libraries(cuexis_judgement_compile
    PUBLIC
        cuexis::judgement
        cuexis::core
)
