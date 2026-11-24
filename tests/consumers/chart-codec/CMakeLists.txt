cmake_minimum_required(VERSION 3.25)

project(CuexisChartCodecConsumer LANGUAGES CXX)

if(NOT DEFINED CUEXIS_SOURCE_DIR)
    message(FATAL_ERROR "CUEXIS_SOURCE_DIR is required")
endif()

# The codec/tools split only exists in the experimental closure, so the consumer enables the
# candidate switch and keeps every unrelated optional target off. The linkage stays static: this
# consumer proves the compile and link closure, not runtime DLL resolution.
set(CUEXIS_ENABLE_CHART_V5_CANDIDATE ON CACHE BOOL "" FORCE)
set(CUEXIS_LIBRARY_TYPE STATIC CACHE STRING "" FORCE)
set(CUEXIS_BUILD_PLAYER OFF CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_SDL_ADAPTER OFF CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_AUDIO_SDL_ADAPTER OFF CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_OPENGL_ADAPTER OFF CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_DEVELOPER_TOOLS OFF CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_MEDIA_TOOLS OFF CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_SHADER_TOOLS OFF CACHE BOOL "" FORCE)
set(CUEXIS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

add_subdirectory("${CUEXIS_SOURCE_DIR}" cuexis)

add_executable(rr_chart_codec_consumer main.cpp)
target_compile_features(rr_chart_codec_consumer PRIVATE cxx_std_20)
#  The pure codec consumer names the codec target and Core only. Base Chart, the shared timing map
#  and the internal Chart timing root arrive through the codec's PUBLIC closure; Chart tools, the
#  Packed Reader, Playback and the author tools are not linked or included here.
target_link_libraries(rr_chart_codec_consumer PRIVATE Cuexis::Core cuexis::chart_codec)

#  Negative translation units, each built in its own configure run. They must fail because the
#  Chart tools include root is not visible from this pure codec consumer:
#
#    - `cxt2_loader`:        CXT2 expansion belongs to cuexis_chart_tools
#    - `candidate_lowering`: candidate source lowering belongs to cuexis_chart_tools
set(CUEXIS_CHART_CODEC_CONSUMER_NEGATIVE "" CACHE STRING
    "Negative translation unit to build: cxt2_loader or candidate_lowering")
if(CUEXIS_CHART_CODEC_CONSUMER_NEGATIVE STREQUAL "cxt2_loader")
    add_executable(rr_chart_codec_negative_cxt2_loader negative_cxt2_loader.cpp)
    target_compile_features(rr_chart_codec_negative_cxt2_loader PRIVATE cxx_std_20)
    target_link_libraries(rr_chart_codec_negative_cxt2_loader PRIVATE Cuexis::Core
                                                                   cuexis::chart_codec)
elseif(CUEXIS_CHART_CODEC_CONSUMER_NEGATIVE STREQUAL "candidate_lowering")
    add_executable(rr_chart_codec_negative_candidate_lowering negative_candidate_lowering.cpp)
    target_compile_features(rr_chart_codec_negative_candidate_lowering PRIVATE cxx_std_20)
    target_link_libraries(rr_chart_codec_negative_candidate_lowering PRIVATE Cuexis::Core
                                                                           cuexis::chart_codec)
elseif(NOT CUEXIS_CHART_CODEC_CONSUMER_NEGATIVE STREQUAL "")
    message(FATAL_ERROR "Unknown CUEXIS_CHART_CODEC_CONSUMER_NEGATIVE value: "
                        "${CUEXIS_CHART_CODEC_CONSUMER_NEGATIVE}")
endif()
