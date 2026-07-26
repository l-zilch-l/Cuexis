add_executable(
    cuexis_player
    src/main.cpp
    src/player_app.cpp
    src/player_app.hpp
    src/player_log.cpp
    src/player_log.hpp
)

find_package(spdlog CONFIG REQUIRED)

target_compile_features(cuexis_player PRIVATE cxx_std_20)

target_link_libraries(
    cuexis_player
    PRIVATE
        cuexis::playback
        cuexis::assets
        cuexis::core
        cuexis::filesystem
        cuexis::platform_sdl
        cuexis::project
        cuexis::render
        cuexis::render_opengl
        spdlog::spdlog
)

file(GLOB_RECURSE CUEXIS_STAGE1C_PROJECT_FILES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/assets/projects/stage1c_project/*"
)

add_custom_target(player_stage1c_assets ALL
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:cuexis_player>/assets/charts"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${PROJECT_SOURCE_DIR}/assets/charts/stage1a_example.cuexis.chart.json"
        "$<TARGET_FILE_DIR:cuexis_player>/assets/charts/stage1a_example.cuexis.chart.json"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${PROJECT_SOURCE_DIR}/assets/charts/stage1a_example.cuexis.chart.simple.json"
        "$<TARGET_FILE_DIR:cuexis_player>/assets/charts/stage1a_example.cuexis.chart.simple.json"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:cuexis_player>/assets/projects"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf
        "$<TARGET_FILE_DIR:cuexis_player>/assets/projects/stage1c_project"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets/projects/stage1c_project"
        "$<TARGET_FILE_DIR:cuexis_player>/assets/projects/stage1c_project"
    DEPENDS
        "${PROJECT_SOURCE_DIR}/assets/charts/stage1a_example.cuexis.chart.json"
        "${PROJECT_SOURCE_DIR}/assets/charts/stage1a_example.cuexis.chart.simple.json"
        ${CUEXIS_STAGE1C_PROJECT_FILES}
    VERBATIM
)

add_dependencies(cuexis_player player_stage1c_assets)
