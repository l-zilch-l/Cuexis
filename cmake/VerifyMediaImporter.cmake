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

# ---------------------------------------------------------------------------------------------
# S6-E3: provenance, cache identity revalidation and generation publication
# ---------------------------------------------------------------------------------------------

media_golden_field(image_rgb8 profile image_profile_identity)
string(REPEAT "0" 64 e3_zero_identity)

set(e3_root "${CUEXIS_MEDIA_IMPORTER_WORK}/e3")
set(e3_artifacts "${e3_root}/artifacts")
set(e3_provenance "${e3_root}/provenance")
set(e3_generations "${e3_root}/generations")
set(e3_cache "${e3_root}/cache")
file(REMOVE_RECURSE "${e3_root}")
file(MAKE_DIRECTORY "${e3_artifacts}" "${e3_provenance}" "${e3_generations}")

execute_process(
    COMMAND "${CUEXIS_MEDIA_IMPORTER}" --kind image --input "${image_fixture}"
        --output-dir "${e3_artifacts}" --asset-id "textures/checker"
        --provenance-dir "${e3_provenance}" --cache-dir "${e3_cache}"
        --generation-dir "${e3_generations}" --generation-id "26.09.26-1" --print-info
    RESULT_VARIABLE e3_status
    OUTPUT_VARIABLE e3_output
    ERROR_VARIABLE e3_error
)
set(e3_text "${e3_output}${e3_error}")
if(NOT e3_status EQUAL 0)
    list(APPEND failures "e3 publish: exited ${e3_status}: ${e3_text}")
endif()

# The author-side provenance sidecar keeps the four identities apart.
set(e3_sidecar "${e3_provenance}/textures%2Fchecker.provenance.json")
if(NOT EXISTS "${e3_sidecar}")
    list(APPEND failures "e3 provenance: expected ${e3_sidecar}")
else()
    file(READ "${e3_sidecar}" e3_provenance_text)
    file(SHA256 "${image_fixture}" e3_source_identity)
    foreach(needle
            "\"format\":\"cuexis.media-provenance\""
            "\"version\":1"
            "\"kind\":\"texture\""
            "\"rawSourceIdentity\":\"${e3_source_identity}\""
            "\"profileIdentity\":\"${image_profile_identity}\""
            "\"artifactIdentity\":\"${image_golden_identity}\""
            "\"decoderVersion\":\"libpng"
            "\"assetId\":\"textures/checker\"")
        string(FIND "${e3_provenance_text}" "${needle}" e3_at)
        if(e3_at EQUAL -1)
            list(APPEND failures
                "e3 provenance: record has no ${needle}: ${e3_provenance_text}")
        endif()
    endforeach()
    # The raw source identity must not be the artifact identity.
    if(e3_source_identity STREQUAL image_golden_identity)
        list(APPEND failures "e3 provenance: the raw source and the artifact share an identity")
    endif()
endif()

# The generation is a complete, content addressed closure: the runtime entry is the golden artifact
# and the provenance sidecar is stored beside the closure rather than inside it.
file(GLOB e3_generation_dirs "${e3_generations}/generations/*")
list(LENGTH e3_generation_dirs e3_generation_count)
if(NOT e3_generation_count EQUAL 1)
    list(APPEND failures "e3 generation: expected one generation, found ${e3_generation_dirs}")
else()
    list(GET e3_generation_dirs 0 e3_generation)
    get_filename_component(e3_generation_identity "${e3_generation}" NAME)
    string(LENGTH "${e3_generation_identity}" e3_generation_name_length)
    string(REGEX MATCH "^[0-9a-f]+$" e3_generation_name_hex "${e3_generation_identity}")
    if(NOT e3_generation_name_length EQUAL 64 OR NOT e3_generation_name_hex)
        list(APPEND failures "e3 generation: directory name is not a SHA-256 (${e3_generation})")
    endif()
    set(e3_runtime_entry "${e3_generation}/textures/textures%2Fchecker.texture.bin")
    if(NOT EXISTS "${e3_runtime_entry}")
        list(APPEND failures "e3 generation: expected runtime entry ${e3_runtime_entry}")
    else()
        file(SHA256 "${e3_runtime_entry}" e3_runtime_identity)
        if(NOT e3_runtime_identity STREQUAL image_golden_identity)
            list(APPEND failures
                "e3 generation: runtime entry ${e3_runtime_identity} is not the golden artifact")
        endif()
    endif()
    if(NOT EXISTS "${e3_generation}/cuexis.generation")
        list(APPEND failures "e3 generation: the generation marker is missing")
    endif()
    if(NOT EXISTS "${e3_generation}/provenance/textures%2Fchecker.provenance.json")
        list(APPEND failures "e3 generation: the provenance record is missing")
    endif()
    file(READ "${e3_generation}/cuexis.generation" e3_marker)
    string(FIND "${e3_marker}" "cuexis.generation 1" e3_marker_format)
    if(e3_marker_format EQUAL -1)
        list(APPEND failures "e3 generation: unexpected marker: ${e3_marker}")
    endif()
endif()

# Staging is transient: a successful publication leaves no staging directory behind.
file(GLOB e3_staging "${e3_generations}/staging/*")
if(e3_staging)
    list(APPEND failures "e3 generation: staging survived publication: ${e3_staging}")
endif()

# Republishing the same batch is an immutable no-op, and the label is part of the recorded identity.
file(GLOB e3_before_dirs "${e3_generations}/generations/*")
execute_process(
    COMMAND "${CUEXIS_MEDIA_IMPORTER}" --kind image --input "${image_fixture}"
        --output-dir "${e3_artifacts}" --asset-id "textures/checker"
        --provenance-dir "${e3_provenance}" --cache-dir "${e3_cache}"
        --generation-dir "${e3_generations}" --generation-id "26.09.26-1"
    RESULT_VARIABLE e3_republish_status
    OUTPUT_VARIABLE e3_republish_output
    ERROR_VARIABLE e3_republish_error
)
if(NOT e3_republish_status EQUAL 0)
    list(APPEND failures
        "e3 republish: exited ${e3_republish_status}: ${e3_republish_output}${e3_republish_error}")
endif()
file(GLOB e3_after_dirs "${e3_generations}/generations/*")
if(NOT e3_before_dirs STREQUAL e3_after_dirs)
    list(APPEND failures "e3 republish: the generation set changed: ${e3_before_dirs} -> ${e3_after_dirs}")
endif()

# A cache hit serves the same artifact without a decoder run, and the cache holds one record.
file(GLOB e3_cache_records "${e3_cache}/records/*.json")
list(LENGTH e3_cache_records e3_record_count)
if(NOT e3_record_count EQUAL 1)
    list(APPEND failures "e3 cache: expected one record, found ${e3_cache_records}")
endif()
execute_process(
    COMMAND "${CUEXIS_MEDIA_IMPORTER}" --kind image --input "${image_fixture}"
        --output-dir "${e3_artifacts}" --asset-id "textures/checker"
        --provenance-dir "${e3_provenance}" --cache-dir "${e3_cache}" --print-info
    RESULT_VARIABLE e3_hit_status
    OUTPUT_VARIABLE e3_hit_output
    ERROR_VARIABLE e3_hit_error
)
if(NOT e3_hit_status EQUAL 0)
    list(APPEND failures "e3 cache hit: exited ${e3_hit_status}: ${e3_hit_output}${e3_hit_error}")
elseif(NOT e3_hit_output MATCHES "\"cached\":true")
    list(APPEND failures "e3 cache hit: the run did not report a cache hit: ${e3_hit_output}")
elseif(NOT e3_hit_output MATCHES "\"artifactIdentity\":\"${image_golden_identity}\"")
    list(APPEND failures "e3 cache hit: the served artifact identity is wrong: ${e3_hit_output}")
endif()

# A damaged record is refused with its own code, and the published artifact is not rewritten.
if(e3_record_count EQUAL 1)
    list(GET e3_cache_records 0 e3_record)
    file(READ "${e3_record}" e3_record_text)
    file(WRITE "${e3_record}" "{\"format\":\"cuexis.media-cache\",\"version\":1,\"key\":\"")
    execute_process(
        COMMAND "${CUEXIS_MEDIA_IMPORTER}" --kind image --input "${image_fixture}"
            --output-dir "${e3_artifacts}" --asset-id "textures/checker"
            --cache-dir "${e3_cache}"
        RESULT_VARIABLE e3_corrupt_status
        OUTPUT_VARIABLE e3_corrupt_output
        ERROR_VARIABLE e3_corrupt_error
    )
    set(e3_corrupt_text "${e3_corrupt_output}${e3_corrupt_error}")
    if(e3_corrupt_status EQUAL 0 OR NOT e3_corrupt_text MATCHES "media.cache.corrupt")
        list(APPEND failures
            "e3 cache corrupt: expected media.cache.corrupt and a nonzero exit;"
            " got status=${e3_corrupt_status}, output=${e3_corrupt_text}")
    endif()
    if(EXISTS "${e3_artifacts}/${image_golden_identity}.texture.bin")
        file(SHA256 "${e3_artifacts}/${image_golden_identity}.texture.bin" e3_artifact_identity)
        if(NOT e3_artifact_identity STREQUAL image_golden_identity)
            list(APPEND failures "e3 cache corrupt: the published artifact changed")
        endif()
    endif()

    # An old profile identity is a different key: the entry is not reused. The record is rewritten
    # with an edited profile, which must be refused instead of silently re-imported.
    file(WRITE "${e3_record}" "${e3_record_text}")
    string(REPLACE "${image_profile_identity}" "${e3_zero_identity}" e3_old_profile_record
        "${e3_record_text}")
    if(e3_old_profile_record STREQUAL e3_record_text)
        list(APPEND failures "e3 old profile: the record does not carry the profile identity")
    else()
        file(WRITE "${e3_record}" "${e3_old_profile_record}")
        execute_process(
            COMMAND "${CUEXIS_MEDIA_IMPORTER}" --kind image --input "${image_fixture}"
                --output-dir "${e3_artifacts}" --asset-id "textures/checker"
                --cache-dir "${e3_cache}"
            RESULT_VARIABLE e3_old_status
            OUTPUT_VARIABLE e3_old_output
            ERROR_VARIABLE e3_old_error
        )
        set(e3_old_text "${e3_old_output}${e3_old_error}")
        if(e3_old_status EQUAL 0 OR NOT e3_old_text MATCHES "media.cache.corrupt")
            list(APPEND failures
                "e3 old profile: expected media.cache.corrupt and a nonzero exit;"
                " got status=${e3_old_status}, output=${e3_old_text}")
        endif()
    endif()

    # Rebuilding is explicit: --rebuild clears the record and re-imports.
    execute_process(
        COMMAND "${CUEXIS_MEDIA_IMPORTER}" --kind image --input "${image_fixture}"
            --output-dir "${e3_artifacts}" --asset-id "textures/checker"
            --cache-dir "${e3_cache}" --rebuild --print-info
        RESULT_VARIABLE e3_rebuild_status
        OUTPUT_VARIABLE e3_rebuild_output
        ERROR_VARIABLE e3_rebuild_error
    )
    if(NOT e3_rebuild_status EQUAL 0)
        list(APPEND failures
            "e3 rebuild: exited ${e3_rebuild_status}: ${e3_rebuild_output}${e3_rebuild_error}")
    endif()
    file(GLOB e3_rebuilt_records "${e3_cache}/records/*.json")
    list(LENGTH e3_rebuilt_records e3_rebuilt_count)
    if(NOT e3_rebuilt_count EQUAL 1)
        list(APPEND failures "e3 rebuild: expected one record after the rebuild, found ${e3_rebuilt_records}")
    endif()
endif()

# A missing raw source fails closed and publishes nothing.
set(e3_missing_dir "${e3_root}/missing")
file(REMOVE_RECURSE "${e3_missing_dir}")
file(MAKE_DIRECTORY "${e3_missing_dir}")
execute_process(
    COMMAND "${CUEXIS_MEDIA_IMPORTER}" --kind image
        --input "${e3_root}/absent.png" --output-dir "${e3_missing_dir}"
        --asset-id "textures/absent" --cache-dir "${e3_cache}"
    RESULT_VARIABLE e3_missing_status
    OUTPUT_VARIABLE e3_missing_output
    ERROR_VARIABLE e3_missing_error
)
if(e3_missing_status EQUAL 0)
    list(APPEND failures "e3 missing source: a missing source succeeded: ${e3_missing_output}${e3_missing_error}")
endif()
file(GLOB e3_missing_files "${e3_missing_dir}/*")
if(e3_missing_files)
    list(APPEND failures "e3 missing source: a failed import published ${e3_missing_files}")
endif()

# A stale lock file is not a lock: publication locks the open handle at the OS level, so a lock file
# left behind by an earlier process must not block a later one. This is the restart recovery case.
set(e3_recovery_dir "${e3_root}/recovery")
file(REMOVE_RECURSE "${e3_recovery_dir}")
file(MAKE_DIRECTORY "${e3_recovery_dir}")
file(WRITE "${e3_recovery_dir}/.cuexis.publish.lock" "")
file(WRITE "${e3_recovery_dir}/staging/abandoned/textures/half.texture" "half")
execute_process(
    COMMAND "${CUEXIS_MEDIA_IMPORTER}" --kind image --input "${image_fixture}"
        --output-dir "${e3_recovery_dir}/artifacts" --asset-id "textures/checker"
        --generation-dir "${e3_recovery_dir}" --generation-id "26.09.26-1"
    RESULT_VARIABLE e3_recovery_status
    OUTPUT_VARIABLE e3_recovery_output
    ERROR_VARIABLE e3_recovery_error
)
if(NOT e3_recovery_status EQUAL 0)
    list(APPEND failures
        "e3 restart recovery: a stale lock file blocked publication: "
        "${e3_recovery_output}${e3_recovery_error}")
endif()
file(GLOB e3_recovery_generations "${e3_recovery_dir}/generations/*")
list(LENGTH e3_recovery_generations e3_recovery_count)
if(NOT e3_recovery_count EQUAL 1)
    list(APPEND failures
        "e3 restart recovery: expected one generation, found ${e3_recovery_generations}")
elseif(NOT e3_generation_count EQUAL 1)
    list(APPEND failures "e3 restart recovery: no serial generation to compare against")
else()
    # The same batch produces the same content addressed identity on both roots.
    list(GET e3_recovery_generations 0 e3_recovery_generation)
    list(GET e3_generation_dirs 0 e3_serial_generation)
    get_filename_component(e3_recovery_name "${e3_recovery_generation}" NAME)
    get_filename_component(e3_serial_name "${e3_serial_generation}" NAME)
    if(NOT e3_recovery_name STREQUAL e3_serial_name)
        list(APPEND failures
            "e3 restart recovery: identity ${e3_recovery_name} differs from ${e3_serial_name}")
    endif()
endif()

if(failures)
    string(REPLACE ";" "\n  - " failure_text "${failures}")
    message(FATAL_ERROR "cuexis_media_importer gate failed:\n  - ${failure_text}")
endif()
