add_executable(
    cuexis_player
    src/main.cpp
    src/frame_diagnostics.cpp
    src/frame_diagnostics.hpp
    src/player_app.cpp
    src/player_app.hpp
    src/player_options.cpp
    src/player_options.hpp
    src/player_surface.hpp
    src/player_audio_seat.hpp
    src/player_assembly.cpp
    src/player_assembly.hpp
    src/player_control.cpp
    src/player_control.hpp
    src/player_smoke.cpp
    src/player_smoke.hpp
    src/player_log.cpp
    src/player_log.hpp
    src/snapshot_scene.cpp
    src/snapshot_scene.hpp
)

find_package(spdlog CONFIG REQUIRED)

target_compile_features(cuexis_player PRIVATE cxx_std_20)

target_link_libraries(
    cuexis_player
    PRIVATE
        cuexis::playback
        cuexis::audio
        cuexis::audio_sdl
        cuexis::core
        cuexis::filesystem
        cuexis::platform_sdl
        cuexis::render
        cuexis::render_opengl
        cuexis::player_support
        spdlog::spdlog
)

file(GLOB_RECURSE CUEXIS_STAGE1B_PROJECT_FILES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/assets/projects/stage1b_project/*"
)
file(GLOB_RECURSE CUEXIS_STAGE1C_PROJECT_FILES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/assets/projects/stage1c_project/*"
)
file(GLOB_RECURSE CUEXIS_STAGE1D_PROJECT_FILES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/assets/projects/stage1d_project/*"
)
file(GLOB_RECURSE CUEXIS_STAGE3_PROJECT_FILES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/assets/projects/stage3_project/*"
)

add_custom_target(player_stage1c_assets ALL
    COMMAND "${CMAKE_COMMAND}" -E rm -rf
        "$<TARGET_FILE_DIR:cuexis_player>/assets/charts"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:cuexis_player>/assets/charts"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${PROJECT_SOURCE_DIR}/assets/charts/stage1a_example.cuexis.chart.json"
        "$<TARGET_FILE_DIR:cuexis_player>/assets/charts/stage1a_example.cuexis.chart.json"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${PROJECT_SOURCE_DIR}/assets/charts/stage2_example.cuexis.chart.json"
        "$<TARGET_FILE_DIR:cuexis_player>/assets/charts/stage2_example.cuexis.chart.json"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:cuexis_player>/assets/projects"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf
        "$<TARGET_FILE_DIR:cuexis_player>/assets/projects/stage1b_project"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets/projects/stage1b_project"
        "$<TARGET_FILE_DIR:cuexis_player>/assets/projects/stage1b_project"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf
        "$<TARGET_FILE_DIR:cuexis_player>/assets/projects/stage1c_project"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets/projects/stage1c_project"
        "$<TARGET_FILE_DIR:cuexis_player>/assets/projects/stage1c_project"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf
        "$<TARGET_FILE_DIR:cuexis_player>/assets/projects/stage1d_project"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets/projects/stage1d_project"
        "$<TARGET_FILE_DIR:cuexis_player>/assets/projects/stage1d_project"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf
        "$<TARGET_FILE_DIR:cuexis_player>/assets/projects/stage3_project"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets/projects/stage3_project"
        "$<TARGET_FILE_DIR:cuexis_player>/assets/projects/stage3_project"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:cuexis_player>/assets/schemas"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${PROJECT_SOURCE_DIR}/schemas/cuexis.player-preferences.v1.schema.json"
        "$<TARGET_FILE_DIR:cuexis_player>/assets/schemas/cuexis.player-preferences.v1.schema.json"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${PROJECT_SOURCE_DIR}/schemas/cuexis.audio-device-profile.v1.schema.json"
        "$<TARGET_FILE_DIR:cuexis_player>/assets/schemas/cuexis.audio-device-profile.v1.schema.json"
    DEPENDS
        "${PROJECT_SOURCE_DIR}/assets/charts/stage1a_example.cuexis.chart.json"
        "${PROJECT_SOURCE_DIR}/assets/charts/stage2_example.cuexis.chart.json"
        ${CUEXIS_STAGE1B_PROJECT_FILES}
        ${CUEXIS_STAGE1C_PROJECT_FILES}
        ${CUEXIS_STAGE1D_PROJECT_FILES}
        ${CUEXIS_STAGE3_PROJECT_FILES}
        "${PROJECT_SOURCE_DIR}/schemas/cuexis.player-preferences.v1.schema.json"
        "${PROJECT_SOURCE_DIR}/schemas/cuexis.audio-device-profile.v1.schema.json"
    VERBATIM
)

add_dependencies(cuexis_player player_stage1c_assets)

# Runnable Player distribution (ADR 0042 S6-D08, stage plan S6-C4 item 3).
# The Player is deliberately not part of the SDK install tree; this target
# assembles a self-contained directory instead.
set(CUEXIS_PLAYER_DIST_ROOT "${PROJECT_BINARY_DIR}/dist" CACHE PATH
    "Directory that receives the packaged Cuexis Player distributions")

get_filename_component(CUEXIS_CXX_COMPILER_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
set(CUEXIS_PLAYER_LIBRARY_DIRS
    "${PROJECT_BINARY_DIR}"
    "${PROJECT_BINARY_DIR}/lib"
    "${PROJECT_BINARY_DIR}/bin"
    "${CUEXIS_CXX_COMPILER_DIR}"
    "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")

add_custom_target(cuexis_player_dist
    COMMAND "${CMAKE_COMMAND}"
        "-DCUEXIS_PLAYER_EXECUTABLE=$<TARGET_FILE:cuexis_player>"
        "-DCUEXIS_PLAYER_ASSETS_DIR=$<TARGET_FILE_DIR:cuexis_player>/assets"
        "-DCUEXIS_PLAYER_DIST_ROOT=${CUEXIS_PLAYER_DIST_ROOT}"
        "-DCUEXIS_PLAYER_LIBRARY_DIRS=${CUEXIS_PLAYER_LIBRARY_DIRS}"
        "-DCUEXIS_VERSION_DISPLAY=${CUEXIS_VERSION_DISPLAY}"
        "-DCUEXIS_SDK_API_VERSION=${CUEXIS_SDK_API_VERSION}"
        "-DCUEXIS_LIBRARY_TYPE=${CUEXIS_LIBRARY_TYPE}"
        "-DCUEXIS_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DCUEXIS_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
        "-DCUEXIS_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}"
        "-DCUEXIS_CXX_COMPILER_ID=${CMAKE_CXX_COMPILER_ID}"
        "-DCUEXIS_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DCUEXIS_VCPKG_SHARE_DIR=${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share"
        -P "${PROJECT_SOURCE_DIR}/cmake/PackagePlayer.cmake"
    DEPENDS cuexis_player player_stage1c_assets
    COMMENT "Assembling the runnable Cuexis Player distribution"
    VERBATIM
)
