add_library(cuexis_execution_host_support STATIC
    host_protocol.cpp host_candidate.cpp audio_owner.cpp frame_observation.cpp capture_route.cpp
    audio_worker.cpp session_worker.cpp)
add_library(cuexis::execution_host_support ALIAS cuexis_execution_host_support)
target_compile_features(cuexis_execution_host_support PUBLIC cxx_std_20)
target_include_directories(cuexis_execution_host_support PRIVATE "${PROJECT_SOURCE_DIR}/engine/core/internal")
target_link_libraries(cuexis_execution_host_support PUBLIC cuexis::playback)
