if(NOT DEFINED CUEXIS_ASSET_IMPORTER)
    message(FATAL_ERROR "CUEXIS_ASSET_IMPORTER is required")
endif()
if(NOT DEFINED CUEXIS_ASSET_IMPORTER_WORK)
    message(FATAL_ERROR "CUEXIS_ASSET_IMPORTER_WORK is required")
endif()

execute_process(
    COMMAND "${CUEXIS_ASSET_IMPORTER}" --help
    RESULT_VARIABLE help_status
    OUTPUT_VARIABLE help_output
    ERROR_VARIABLE help_error
)
if(NOT help_status EQUAL 0)
    message(FATAL_ERROR
        "cuexis_asset_importer --help exited ${help_status}\n${help_output}\n${help_error}")
endif()

set(help_text "${help_output}${help_error}")
if(NOT help_text MATCHES "Usage: cuexis_asset_importer")
    message(FATAL_ERROR "cuexis_asset_importer --help did not print usage:\n${help_text}")
endif()
if(NOT help_text MATCHES "compile")
    message(FATAL_ERROR
        "cuexis_asset_importer --help did not mention --compile:\n${help_text}")
endif()

execute_process(
    COMMAND "${CUEXIS_ASSET_IMPORTER}"
    RESULT_VARIABLE usage_status
    OUTPUT_VARIABLE usage_output
    ERROR_VARIABLE usage_error
)
if(NOT usage_status EQUAL 2)
    message(FATAL_ERROR
        "cuexis_asset_importer without arguments exited ${usage_status}, expected 2\n"
        "${usage_output}\n${usage_error}")
endif()

file(MAKE_DIRECTORY "${CUEXIS_ASSET_IMPORTER_WORK}")
set(write_glsl "${CUEXIS_ASSET_IMPORTER_WORK}/write_lf.py")
file(WRITE "${write_glsl}"
    "from pathlib import Path\n"
    "import sys\n"
    "Path(sys.argv[1]).write_bytes(sys.argv[2].replace('|', '\\n').encode('utf-8'))\n"
)

set(vertex_glsl "${CUEXIS_ASSET_IMPORTER_WORK}/passthrough.vert.glsl")
set(fragment_glsl "${CUEXIS_ASSET_IMPORTER_WORK}/passthrough.frag.glsl")
set(include_glsl "${CUEXIS_ASSET_IMPORTER_WORK}/include.vert.glsl")

string(CONCAT vertex_body
    "#version 450|"
    "layout(std140, set = 0, binding = 0) uniform CuexisObject {|"
    "    mat4 world;|"
    "    mat4 viewProjection;|"
    "    vec3 tint;|"
    "    float opacity;|"
    "} cuexisObject;|"
    "layout(location = 0) in vec3 aPosition;|"
    "void main() {|"
    "    gl_Position = cuexisObject.viewProjection * cuexisObject.world * vec4(aPosition, 1.0);|"
    "}|"
)
string(CONCAT fragment_body
    "#version 450|"
    "layout(std140, set = 0, binding = 0) uniform CuexisObject {|"
    "    mat4 world;|"
    "    mat4 viewProjection;|"
    "    vec3 tint;|"
    "    float opacity;|"
    "} cuexisObject;|"
    "layout(location = 0) out vec4 outColor;|"
    "void main() {|"
    "    outColor = vec4(cuexisObject.tint, cuexisObject.opacity);|"
    "}|"
)
set(include_body "#version 450|#include \"stolen.glsl\"|void main() {}|")

find_program(CUEXIS_PYTHON NAMES python python3 REQUIRED)
execute_process(COMMAND "${CUEXIS_PYTHON}" "${write_glsl}" "${vertex_glsl}" "${vertex_body}"
    RESULT_VARIABLE write_vertex_status)
if(NOT write_vertex_status EQUAL 0)
    message(FATAL_ERROR "Failed to write LF vertex fixture")
endif()
execute_process(COMMAND "${CUEXIS_PYTHON}" "${write_glsl}" "${fragment_glsl}" "${fragment_body}"
    RESULT_VARIABLE write_fragment_status)
if(NOT write_fragment_status EQUAL 0)
    message(FATAL_ERROR "Failed to write LF fragment fixture")
endif()
execute_process(COMMAND "${CUEXIS_PYTHON}" "${write_glsl}" "${include_glsl}" "${include_body}"
    RESULT_VARIABLE write_include_status)
if(NOT write_include_status EQUAL 0)
    message(FATAL_ERROR "Failed to write LF include fixture")
endif()

execute_process(
    COMMAND "${CUEXIS_ASSET_IMPORTER}" --compile
        --vertex "${vertex_glsl}" --fragment "${fragment_glsl}"
    RESULT_VARIABLE compile_status
    OUTPUT_VARIABLE compile_output
    ERROR_VARIABLE compile_error
)
if(NOT compile_status EQUAL 0)
    message(FATAL_ERROR
        "cuexis_asset_importer --compile failed (${compile_status})\n"
        "${compile_output}\n${compile_error}")
endif()
if(NOT compile_output MATCHES "status=ok")
    message(FATAL_ERROR
        "cuexis_asset_importer --compile did not report status=ok:\n${compile_output}")
endif()
if(NOT compile_output MATCHES "has_cuexis_object=1")
    message(FATAL_ERROR
        "cuexis_asset_importer --compile did not report CuexisObject:\n${compile_output}")
endif()
if(NOT compile_output MATCHES "vertex_spirv_bytes=")
    message(FATAL_ERROR
        "cuexis_asset_importer --compile did not report SPIR-V sizes:\n${compile_output}")
endif()
if(compile_output MATCHES "cache_written=1" OR compile_error MATCHES "cache_written=1")
    message(FATAL_ERROR "importer --compile without --cache-dir wrote a cache file")
endif()

set(cache_dir "${CUEXIS_ASSET_IMPORTER_WORK}/cache")
file(REMOVE_RECURSE "${cache_dir}")
execute_process(
    COMMAND "${CUEXIS_ASSET_IMPORTER}" --compile
        --vertex "${vertex_glsl}" --fragment "${fragment_glsl}"
        --cache-dir "${cache_dir}"
    RESULT_VARIABLE cache_status
    OUTPUT_VARIABLE cache_output
    ERROR_VARIABLE cache_error
)
if(NOT cache_status EQUAL 0)
    message(FATAL_ERROR
        "cuexis_asset_importer --compile --cache-dir failed (${cache_status})\n"
        "${cache_output}\n${cache_error}")
endif()
if(NOT cache_output MATCHES "cache_written=1")
    message(FATAL_ERROR
        "cuexis_asset_importer --cache-dir did not report cache_written=1:\n${cache_output}")
endif()

file(GLOB cache_files "${cache_dir}/*.cxscch01")
list(LENGTH cache_files cache_count)
if(NOT cache_count EQUAL 1)
    message(FATAL_ERROR
        "expected one CXSCCH01 file in ${cache_dir}, found ${cache_count}")
endif()
list(GET cache_files 0 cache_file)
file(READ "${cache_file}" cache_bytes HEX)
string(SUBSTRING "${cache_bytes}" 0 16 cache_magic_hex)
if(NOT cache_magic_hex STREQUAL "4358534343483031")
    message(FATAL_ERROR "cache file magic is not CXSCCH01: ${cache_magic_hex}")
endif()
file(READ "${cache_file}" first_cache HEX)

execute_process(
    COMMAND "${CUEXIS_ASSET_IMPORTER}" --compile
        --vertex "${vertex_glsl}" --fragment "${fragment_glsl}"
        --cache-dir "${cache_dir}"
    RESULT_VARIABLE cache_again_status
    OUTPUT_VARIABLE cache_again_output
    ERROR_VARIABLE cache_again_error
)
if(NOT cache_again_status EQUAL 0)
    message(FATAL_ERROR
        "second --cache-dir compile failed (${cache_again_status})\n"
        "${cache_again_output}\n${cache_again_error}")
endif()
file(READ "${cache_file}" second_cache HEX)
if(NOT first_cache STREQUAL second_cache)
    message(FATAL_ERROR "second CXSCCH01 write did not reproduce the first cache bytes")
endif()

file(REMOVE "${cache_file}")
execute_process(
    COMMAND "${CUEXIS_ASSET_IMPORTER}" --compile
        --vertex "${vertex_glsl}" --fragment "${fragment_glsl}"
        --cache-dir "${cache_dir}"
    RESULT_VARIABLE cache_rebuild_status
    OUTPUT_VARIABLE cache_rebuild_output
    ERROR_VARIABLE cache_rebuild_error
)
if(NOT cache_rebuild_status EQUAL 0)
    message(FATAL_ERROR
        "rebuild --cache-dir compile failed (${cache_rebuild_status})\n"
        "${cache_rebuild_output}\n${cache_rebuild_error}")
endif()
file(READ "${cache_file}" rebuilt_cache HEX)
if(NOT first_cache STREQUAL rebuilt_cache)
    message(FATAL_ERROR "deleted CXSCCH01 rebuild did not match the original bytes")
endif()

execute_process(
    COMMAND "${CUEXIS_ASSET_IMPORTER}" --compile
        --vertex "${include_glsl}" --fragment "${fragment_glsl}"
    RESULT_VARIABLE include_status
    OUTPUT_VARIABLE include_output
    ERROR_VARIABLE include_error
)
if(NOT include_status EQUAL 1)
    message(FATAL_ERROR
        "cuexis_asset_importer --compile of #include exited ${include_status}, expected 1\n"
        "${include_output}\n${include_error}")
endif()
set(include_text "${include_output}${include_error}")
if(NOT include_text MATCHES "playback.presentation.shader.subset_invalid")
    message(FATAL_ERROR
        "cuexis_asset_importer --compile of #include did not use subset_invalid:\n${include_text}")
endif()
