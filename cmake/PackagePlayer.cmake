# Assembles a runnable Cuexis Player distribution directory.
#
# The Player is not part of the SDK install tree (ADR 0042 S6-D08). This script
# produces a self-contained directory that contains the Player executable, the
# runtime libraries it links, its license set, its version metadata and the
# default resource location, so the Player can start without the development
# machine PATH and without the source tree's assets directory.
#
# Required variables: CUEXIS_PLAYER_EXECUTABLE, CUEXIS_PLAYER_ASSETS_DIR,
# CUEXIS_PLAYER_DIST_ROOT, CUEXIS_VERSION_DISPLAY, CUEXIS_SDK_API_VERSION,
# CUEXIS_LIBRARY_TYPE, CUEXIS_BUILD_TYPE, CUEXIS_SYSTEM_NAME,
# CUEXIS_SYSTEM_PROCESSOR, CUEXIS_CXX_COMPILER_ID, CUEXIS_SOURCE_DIR,
# CUEXIS_VCPKG_SHARE_DIR.

foreach(required IN ITEMS
        CUEXIS_PLAYER_EXECUTABLE
        CUEXIS_PLAYER_ASSETS_DIR
        CUEXIS_PLAYER_DIST_ROOT
        CUEXIS_VERSION_DISPLAY
        CUEXIS_LIBRARY_TYPE
        CUEXIS_BUILD_TYPE)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
if(NOT EXISTS "${CUEXIS_PLAYER_EXECUTABLE}")
    message(FATAL_ERROR "The Player executable does not exist: ${CUEXIS_PLAYER_EXECUTABLE}")
endif()
if(NOT IS_DIRECTORY "${CUEXIS_PLAYER_ASSETS_DIR}")
    message(FATAL_ERROR "The Player resource directory does not exist: ${CUEXIS_PLAYER_ASSETS_DIR}")
endif()

set(distribution_name
    "cuexis-player-${CUEXIS_VERSION_DISPLAY}-${CUEXIS_SYSTEM_NAME}-${CUEXIS_LIBRARY_TYPE}-${CUEXIS_BUILD_TYPE}")
string(TOLOWER "${distribution_name}" distribution_name)
set(dist_dir "${CUEXIS_PLAYER_DIST_ROOT}/${distribution_name}")
file(REMOVE_RECURSE "${dist_dir}")
file(MAKE_DIRECTORY "${dist_dir}")

# ---------------------------------------------------------------------------
# 1. Executable and its runtime library closure.
# ---------------------------------------------------------------------------
file(COPY "${CUEXIS_PLAYER_EXECUTABLE}" DESTINATION "${dist_dir}")
get_filename_component(player_name "${CUEXIS_PLAYER_EXECUTABLE}" NAME)
get_filename_component(player_dir "${CUEXIS_PLAYER_EXECUTABLE}" DIRECTORY)

set(search_directories "${player_dir}")
if(DEFINED CUEXIS_PLAYER_LIBRARY_DIRS AND NOT "${CUEXIS_PLAYER_LIBRARY_DIRS}" STREQUAL "")
    list(APPEND search_directories ${CUEXIS_PLAYER_LIBRARY_DIRS})
endif()
list(REMOVE_DUPLICATES search_directories)

file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${CUEXIS_PLAYER_EXECUTABLE}"
    RESOLVED_DEPENDENCIES_VAR player_dependencies
    UNRESOLVED_DEPENDENCIES_VAR player_unresolved
    DIRECTORIES ${search_directories}
    PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-" "^[Aa][Pp][Ii][-_]"
    POST_EXCLUDE_REGEXES
        ".*[/\\\\][Ww]indows[/\\\\][Ss]ystem32[/\\\\].*"
        ".*[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\].*"
        ".*[/\\\\][Ss]ystem32[/\\\\].*"
        "^/lib.*"
        "^/usr/lib.*")

set(copied_runtime_libraries "")
foreach(dependency IN LISTS player_dependencies)
    if(NOT EXISTS "${dependency}")
        continue()
    endif()
    get_filename_component(dependency_name "${dependency}" NAME)
    # Only redistribute what the build produced or fetched; never a system
    # library.
    if(CUEXIS_LIBRARY_TYPE STREQUAL "STATIC" AND dependency_name MATCHES "^cuexis")
        continue()
    endif()
    file(COPY "${dependency}" DESTINATION "${dist_dir}")
    list(APPEND copied_runtime_libraries "${dependency_name}")
endforeach()
list(SORT copied_runtime_libraries)
list(LENGTH copied_runtime_libraries runtime_library_count)

# On Windows, third-party runtime libraries live next to the executable even for
# a static Cuexis build, because vcpkg builds them dynamically. Copying the
# remaining DLLs next to the Player keeps the distribution runnable.
if(WIN32 OR CUEXIS_SYSTEM_NAME STREQUAL "Windows")
    file(GLOB player_directory_libraries "${player_dir}/*.dll")
    foreach(library IN LISTS player_directory_libraries)
        get_filename_component(library_name "${library}" NAME)
        if(library_name STREQUAL "cuexis_architecture_tests.dll")
            continue()
        endif()
        list(FIND copied_runtime_libraries "${library_name}" already_copied)
        if(already_copied EQUAL -1)
            file(COPY "${library}" DESTINATION "${dist_dir}")
            list(APPEND copied_runtime_libraries "${library_name}")
        endif()
    endforeach()
    list(SORT copied_runtime_libraries)
    list(LENGTH copied_runtime_libraries runtime_library_count)
endif()

# ---------------------------------------------------------------------------
# 2. Default resource location.
# ---------------------------------------------------------------------------
file(COPY "${CUEXIS_PLAYER_ASSETS_DIR}/" DESTINATION "${dist_dir}/assets")

# ---------------------------------------------------------------------------
# 3. Licenses and notices.
# ---------------------------------------------------------------------------
foreach(notice IN ITEMS LICENSE NOTICE THIRD_PARTY_NOTICES.md)
    if(EXISTS "${CUEXIS_SOURCE_DIR}/${notice}")
        file(COPY "${CUEXIS_SOURCE_DIR}/${notice}" DESTINATION "${dist_dir}")
    endif()
endforeach()

set(required_license_ports
    entt
    fmt
    glad
    glm
    json-schema-validator
    minizip-ng
    nlohmann-json
    sdl3
    spdlog
    tl-expected)
# Note: the vcpkg port that builds nlohmann_json_schema_validator.dll is a
# separate port whose share directory carries no copyright file; its license
# text is the json-schema-validator text above, for the same upstream project.
file(MAKE_DIRECTORY "${dist_dir}/licenses")
foreach(port IN LISTS required_license_ports)
    set(source "${CUEXIS_VCPKG_SHARE_DIR}/${port}/copyright")
    if(NOT EXISTS "${source}")
        message(FATAL_ERROR
            "The Player distribution is missing the ${port} license text at ${source}")
    endif()
    file(COPY "${source}" DESTINATION "${dist_dir}/licenses"
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    file(RENAME "${dist_dir}/licenses/copyright" "${dist_dir}/licenses/${port}-copyright.txt")
endforeach()

# ---------------------------------------------------------------------------
# 4. Version metadata.
# ---------------------------------------------------------------------------
file(WRITE "${dist_dir}/VERSION.txt"
    "format=cuexis.player-distribution\n"
    "version=1\n"
    "distribution=${distribution_name}\n"
    "display_version=${CUEXIS_VERSION_DISPLAY}\n"
    "sdk_api_version=${CUEXIS_SDK_API_VERSION}\n"
    "library_type=${CUEXIS_LIBRARY_TYPE}\n"
    "build_type=${CUEXIS_BUILD_TYPE}\n"
    "system_name=${CUEXIS_SYSTEM_NAME}\n"
    "system_processor=${CUEXIS_SYSTEM_PROCESSOR}\n"
    "compiler=${CUEXIS_CXX_COMPILER_ID}\n"
    "executable=${player_name}\n"
    "resources=assets\n"
    "runtime_libraries=${runtime_library_count}\n")

file(WRITE "${dist_dir}/README.txt"
    "Cuexis Player distribution\n"
    "\n"
    "This directory is self-contained. Start ${player_name} from this directory:\n"
    "the Player resolves its runtime libraries from here and its default resources\n"
    "from ./assets, so neither a development PATH nor a Cuexis source checkout is\n"
    "required.\n"
    "\n"
    "  VERSION.txt   build and flavor metadata (library type, build type, versions)\n"
    "  assets/       default resource location (charts, projects, schemas)\n"
    "  licenses/     third-party license texts redistributed with this build\n"
    "  README.txt    this file\n"
    "\n"
    "One distribution directory is one flavor: a static and a shared build, or a\n"
    "Debug and a Release build, must not be merged into a single directory.\n"
    "\n"
    "The Player still requires a window and a GPU for --smoke-test. Argument and\n"
    "content failures are reported with a stable player.* diagnostic code.\n")

# ---------------------------------------------------------------------------
# 5. Record the produced directory for the packaging gate.
# ---------------------------------------------------------------------------
file(WRITE "${CUEXIS_PLAYER_DIST_ROOT}/cuexis-player-dist.txt"
    "distribution=${distribution_name}\n"
    "path=${dist_dir}\n"
    "flavor=${CUEXIS_LIBRARY_TYPE}-${CUEXIS_BUILD_TYPE}\n")

message(STATUS "Cuexis Player distribution: ${dist_dir} (${runtime_library_count} runtime libraries)")
