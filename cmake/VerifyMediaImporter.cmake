# S6-E1/S6-E2 media importer end-to-end gate.
#
# Verifies the CLI boundary of the default-OFF media-tools artifact:
#   * usage and exit codes,
#   * worker-mode and in-process conversion producing the same content-addressed artifact,
#   * the artifact name is the SHA-256 of its own bytes and matches the committed golden,
#   * republishing is immutable: identical bytes are a no-op, different bytes are a conflict,
#   * a process memory limit below the decode requirement fails closed and publishes nothing.
#
# The gate never runs a decoder inside Playback or Player; it only drives the offline importer.

if(NOT DEFINED CUEXIS_MEDIA_IMPORTER)
    message(FATAL_ERROR "CUEXIS_MEDIA_IMPORTER is required")
endif()
if(NOT DEFINED CUEXIS_MEDIA_IMPORTER_WORK)
    message(FATAL_ERROR "CUEXIS_MEDIA_IMPORTER_WORK is required")
endif()
if(NOT DEFINED CUEXIS_MEDIA_FIXTURES)
    message(FATAL_ERROR "CUEXIS_MEDIA_FIXTURES is required")
endif()

set(failures)

function(media_golden_field stem field output)
    file(READ "${CUEXIS_MEDIA_FIXTURES}/../golden/${stem}.json" document)
    string(REGEX MATCH "\"${field}\": \"([0-9a-f]+)\"" matched "${document}")
    if(NOT matched)
        message(FATAL_ERROR "golden ${stem}.json has no string field ${field}")
    endif()
    set(${output} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------------------------
# Usage and exit codes
# ---------------------------------------------------------------------------------------------

execute_process(
    COMMAND "${CUEXIS_MEDIA_IMPORTER}" --help
    RESULT_VARIABLE help_status
    OUTPUT_VARIABLE help_output
    ERROR_VARIABLE help_error
)
set(help_text "${help_output}${help_error}")
if(NOT help_status EQUAL 0)
    list(APPEND failures "help: --help exited ${help_status}: ${help_text}")
elseif(NOT help_text MATCHES "usage: cuexis_media_importer")
    list(APPEND failures "help: --help did not print usage: ${help_text}")
elseif(NOT help_text MATCHES "--worker")
    list(APPEND failures "help: --help did not mention --worker: ${help_text}")
elseif(NOT help_text MATCHES "--memory-limit")
    list(APPEND failures "help: --help did not mention --memory-limit: ${help_text}")
endif()

execute_process(
    COMMAND "${CUEXIS_MEDIA_IMPORTER}"
    RESULT_VARIABLE usage_status
    OUTPUT_VARIABLE usage_output
    ERROR_VARIABLE usage_error
)
if(NOT usage_status EQUAL 1)
    list(APPEND failures
        "usage: no arguments exited ${usage_status}, expected 1: ${usage_output}${usage_error}")
endif()

execute_process(
    COMMAND "${CUEXIS_MEDIA_IMPORTER}" --kind video --input ignored --output-dir ignored
    RESULT_VARIABLE kind_status
    OUTPUT_VARIABLE kind_output
    ERROR_VARIABLE kind_error
)
if(NOT kind_status EQUAL 1)
    list(APPEND failures
        "usage: unknown kind exited ${kind_status}, expected 1: ${kind_output}${kind_error}")
endif()

# ---------------------------------------------------------------------------------------------
# Worker mode and in-process mode must produce the same content-addressed artifact
# ---------------------------------------------------------------------------------------------

set(worker_dir "${CUEXIS_MEDIA_IMPORTER_WORK}/worker")
set(in_process_dir "${CUEXIS_MEDIA_IMPORTER_WORK}/in-process")
file(REMOVE_RECURSE "${worker_dir}" "${in_process_dir}")
file(MAKE_DIRECTORY "${worker_dir}" "${in_process_dir}")

set(image_fixture "${CUEXIS_MEDIA_FIXTURES}/image/rgb8.png")
media_golden_field(image_rgb8 sha256 image_golden_identity)

execute_process(
    COMMAND "${CUEXIS_MEDIA_IMPORTER}" --kind image --input "${image_fixture}"
        --output-dir "${worker_dir}" --print-info
    RESULT_VARIABLE worker_status
    OUTPUT_VARIABLE worker_output
    ERROR_VARIABLE worker_error
)
if(NOT worker_status EQUAL 0)
    list(APPEND failures "image worker: exited ${worker_status}: ${worker_output}${worker_error}")
endif()
if(NOT worker_output MATCHES "\"kind\":\"image\"")
    list(APPEND failures "image worker: --print-info did not report an image: ${worker_output}")
endif()

set(worker_artifact "${worker_dir}/${image_golden_identity}.texture.bin")
if(NOT EXISTS "${worker_artifact}")
    list(APPEND failures "image worker: expected ${worker_artifact}")
    file(GLOB worker_files "${worker_dir}/*")
    list(APPEND failures "image worker: directory holds ${worker_files}")
else()
    file(SHA256 "${worker_artifact}" worker_identity)
    if(NOT worker_identity STREQUAL image_golden_identity)
        list(APPEND failures
            "image worker: artifact ${worker_identity} does not match golden ${image_golden_identity}")
    endif()
    file(READ "${worker_artifact}" worker_hex HEX)
    string(SUBSTRING "${worker_hex}" 0 16 worker_magic)
    if(NOT worker_magic STREQUAL "4358505245533031")
        list(APPEND failures "image worker: artifact magic is not CXPRES01 (${worker_magic})")
    endif()
endif()

execute_process(
    COMMAND "${CUEXIS_MEDIA_IMPORTER}" --kind image --input "${image_fixture}"
        --output-dir "${in_process_dir}" --in-process --print-info
    RESULT_VARIABLE in_process_status
    OUTPUT_VARIABLE in_process_output
    ERROR_VARIABLE in_process_error
)
if(NOT in_process_status EQUAL 0)
    list(APPEND failures
        "image in-process: exited ${in_process_status}: ${in_process_output}${in_process_error}")
endif()

set(in_process_artifact "${in_process_dir}/${image_golden_identity}.texture.bin")
if(NOT EXISTS "${in_process_artifact}")
    list(APPEND failures "image in-process: expected ${in_process_artifact}")
elseif(EXISTS "${worker_artifact}")
    file(SHA256 "${in_process_artifact}" in_process_identity)
    if(NOT in_process_identity STREQUAL worker_identity)
        list(APPEND failures
            "worker and in-process bytes differ: ${worker_identity} vs ${in_process_identity}")
    endif()
endif()
string(REGEX MATCH "\\{[^\\}]*\\}" worker_json "${worker_output}")
string(REGEX MATCH "\\{[^\\}]*\\}" in_process_json "${in_process_output}")
if(NOT worker_json STREQUAL in_process_json)
    list(APPEND failures
        "worker and in-process info differ:\n${worker_output}\n${in_process_output}")
endif()

# A worker run must not leave its temporary file behind.
file(GLOB worker_temporaries "${worker_dir}/*.tmp" "${in_process_dir}/*.tmp")
if(worker_temporaries)
    list(APPEND failures "temporary files survived publication: ${worker_temporaries}")
endif()

# ---------------------------------------------------------------------------------------------
# Republishing is immutable
# ---------------------------------------------------------------------------------------------

if(EXISTS "${worker_artifact}")
    file(READ "${worker_artifact}" published_before HEX)
    execute_process(
        COMMAND "${CUEXIS_MEDIA_IMPORTER}" --kind image --input "${image_fixture}"
            --output-dir "${worker_dir}"
        RESULT_VARIABLE republish_status
        OUTPUT_VARIABLE republish_output
        ERROR_VARIABLE republish_error
    )
    file(READ "${worker_artifact}" published_after HEX)
    if(NOT republish_status EQUAL 0)
        list(APPEND failures
            "republish: identical bytes exited ${republish_status}: "
            "${republish_output}${republish_error}")
    endif()
    if(NOT published_before STREQUAL published_after)
        list(APPEND failures "republish: identical bytes changed the published artifact")
    endif()

    # A published artifact whose bytes no longer match its content address must be reported, not
    # overwritten.
    file(WRITE "${worker_artifact}" "tampered")
    execute_process(
        COMMAND "${CUEXIS_MEDIA_IMPORTER}" --kind image --input "${image_fixture}"
            --output-dir "${worker_dir}"
        RESULT_VARIABLE conflict_status
        OUTPUT_VARIABLE conflict_output
        ERROR_VARIABLE conflict_error
    )
    set(conflict_text "${conflict_output}${conflict_error}")
    file(READ "${worker_artifact}" conflicted_after HEX)
    if(conflict_status EQUAL 0 OR NOT conflict_text MATCHES "media.publish.immutable_conflict")
        list(APPEND failures
            "immutable conflict: expected media.publish.immutable_conflict and a nonzero exit;"
            " got status=${conflict_status}, output=${conflict_text}")
    endif()
    if(NOT conflicted_after STREQUAL "74616d7065726564")
        list(APPEND failures "immutable conflict: the existing artifact was overwritten")
    endif()
    file(REMOVE "${worker_artifact}")
endif()

# ---------------------------------------------------------------------------------------------
# A tiny process memory limit fails closed and publishes nothing
# ---------------------------------------------------------------------------------------------

# A 1024x1024 source needs at least 4 MiB of decoded RGBA8, so a 1 MiB process cap must fail closed
# on every platform. Sanitized builds cannot apply a POSIX address-space cap at all, so the case is
# skipped there and stays covered by the non-sanitized presets.
if(CUEXIS_MEDIA_IMPORTER_SANITIZED)
    message(STATUS "media importer gate: skipping the memory-limit case in a sanitized build")
else()
    set(limited_dir "${CUEXIS_MEDIA_IMPORTER_WORK}/limited")
    file(REMOVE_RECURSE "${limited_dir}")
    file(MAKE_DIRECTORY "${limited_dir}")
    execute_process(
        COMMAND "${CUEXIS_MEDIA_IMPORTER}" --kind image
            --input "${CUEXIS_MEDIA_FIXTURES}/image/budget_1024.png"
            --output-dir "${limited_dir}" --memory-limit 1048576
        RESULT_VARIABLE limited_status
        OUTPUT_VARIABLE limited_output
        ERROR_VARIABLE limited_error
    )
    if(limited_status EQUAL 0)
        list(APPEND failures
            "memory limit: a 1 MiB process limit succeeded: ${limited_output}${limited_error}")
    endif()
    file(GLOB limited_files "${limited_dir}/*")
    if(limited_files)
        list(APPEND failures "memory limit: a failed import published ${limited_files}")
    endif()
endif()

# ---------------------------------------------------------------------------------------------
# Audio: worker mode, canonical WAV layout, and golden identity
# ---------------------------------------------------------------------------------------------

set(audio_dir "${CUEXIS_MEDIA_IMPORTER_WORK}/audio")
file(REMOVE_RECURSE "${audio_dir}")
file(MAKE_DIRECTORY "${audio_dir}")
media_golden_field(audio_mono_flac sha256 audio_golden_identity)
execute_process(
    COMMAND "${CUEXIS_MEDIA_IMPORTER}" --kind audio
        --input "${CUEXIS_MEDIA_FIXTURES}/audio/mono.flac"
        --output-dir "${audio_dir}" --print-info
    RESULT_VARIABLE audio_status
    OUTPUT_VARIABLE audio_output
    ERROR_VARIABLE audio_error
)
set(audio_artifact "${audio_dir}/${audio_golden_identity}.wav")
if(NOT audio_status EQUAL 0)
    list(APPEND failures "audio worker: exited ${audio_status}: ${audio_output}${audio_error}")
elseif(NOT EXISTS "${audio_artifact}")
    list(APPEND failures "audio worker: expected ${audio_artifact}")
else()
    file(SHA256 "${audio_artifact}" audio_identity)
    if(NOT audio_identity STREQUAL audio_golden_identity)
        list(APPEND failures
            "audio worker: artifact ${audio_identity} does not match golden ${audio_golden_identity}")
    endif()
    file(READ "${audio_artifact}" audio_hex HEX)
    string(SUBSTRING "${audio_hex}" 0 8 audio_magic)
    if(NOT audio_magic STREQUAL "52494646")
        list(APPEND failures "audio worker: artifact magic is not RIFF (${audio_magic})")
    endif()
    string(LENGTH "${audio_hex}" audio_hex_length)
    math(EXPR audio_bytes "${audio_hex_length} / 2")
    if(NOT audio_bytes EQUAL 1324)
        list(APPEND failures "audio worker: canonical WAV is ${audio_bytes} bytes, expected 1324")
    endif()
endif()
if(NOT audio_output MATCHES "\"kind\":\"audio\"")
    list(APPEND failures "audio worker: --print-info did not report audio: ${audio_output}")
endif()

# ---------------------------------------------------------------------------------------------
# A rejected source exits 2, names its reason, and publishes nothing
# ---------------------------------------------------------------------------------------------

set(reject_dir "${CUEXIS_MEDIA_IMPORTER_WORK}/reject")
file(REMOVE_RECURSE "${reject_dir}")
file(MAKE_DIRECTORY "${reject_dir}")
execute_process(
    COMMAND "${CUEXIS_MEDIA_IMPORTER}" --kind image
        --input "${CUEXIS_MEDIA_FIXTURES}/image/cmyk.jpg" --output-dir "${reject_dir}"
    RESULT_VARIABLE reject_status
    OUTPUT_VARIABLE reject_output
    ERROR_VARIABLE reject_error
)
set(reject_text "${reject_output}${reject_error}")
if(NOT reject_status EQUAL 2)
    list(APPEND failures "reject: CMYK JPEG exited ${reject_status}, expected 2: ${reject_text}")
endif()
if(NOT reject_text MATCHES "media.image.color_type_unsupported")
    list(APPEND failures "reject: CMYK JPEG did not report color_type_unsupported: ${reject_text}")
endif()
file(GLOB reject_files "${reject_dir}/*")
if(reject_files)
    list(APPEND failures "reject: a failed import published ${reject_files}")
endif()

if(failures)
    string(REPLACE ";" "\n  - " failure_text "${failures}")
    message(FATAL_ERROR "cuexis_media_importer gate failed:\n  - ${failure_text}")
endif()
