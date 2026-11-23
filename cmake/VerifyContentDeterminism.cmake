cmake_minimum_required(VERSION 3.25)

foreach(required_variable IN ITEMS
        CUEXIS_CXC_PACK
        CUEXIS_CHART_MIGRATOR
        CUEXIS_HEADLESS_CONSUMER
        CUEXIS_SOURCE_DIR
        CUEXIS_BINARY_DIR
        CUEXIS_IMPLEMENTATION_SHA)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

string(TOLOWER "${CUEXIS_IMPLEMENTATION_SHA}" implementation_sha)
string(LENGTH "${implementation_sha}" implementation_sha_length)
if(NOT implementation_sha_length EQUAL 40 OR NOT implementation_sha MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "CUEXIS_IMPLEMENTATION_SHA must be a full hexadecimal Git SHA")
endif()

set(fixture_root "${CUEXIS_SOURCE_DIR}/tests/fixtures/chart_format_update")
set(reference_project "${fixture_root}/cfu_f_reference_project")
set(golden_root "${fixture_root}/golden")
set(golden_package "${golden_root}/cfu_f_v4_reference.cxc")
set(golden_migration "${golden_root}/chart_v4_static_migration.canonical.json")
set(golden_fingerprint "${golden_root}/cfu_f3_determinism.txt")
set(migration_source "${fixture_root}/valid/chart_v3_static_migration.json")

set(evidence_root "${CUEXIS_BINARY_DIR}/cfu-f3")
file(REMOVE_RECURSE "${evidence_root}")
file(MAKE_DIRECTORY "${evidence_root}")

function(run_checked label stdout_path stderr_path)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE command_result
        OUTPUT_VARIABLE command_output
        ERROR_VARIABLE command_error
    )
    file(WRITE "${stdout_path}" "${command_output}")
    file(WRITE "${stderr_path}" "${command_error}")
    if(NOT command_result EQUAL 0)
        message(FATAL_ERROR
            "${label} failed with exit code ${command_result}\n"
            "stdout:\n${command_output}\n"
            "stderr:\n${command_error}")
    endif()
    set(CFU_F3_LAST_STDOUT "${command_output}" PARENT_SCOPE)
endfunction()

function(assert_same_file actual expected label)
    if(NOT EXISTS "${actual}" OR NOT EXISTS "${expected}")
        message(FATAL_ERROR "${label}: one of the compared files does not exist")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files "${actual}" "${expected}"
        RESULT_VARIABLE compare_result
    )
    if(NOT compare_result EQUAL 0)
        message(FATAL_ERROR "${label}: files differ")
    endif()
endfunction()

function(write_lf path contents)
    set(native_path "${path}.native")
    file(WRITE "${native_path}" "${contents}")
    configure_file("${native_path}" "${path}" @ONLY NEWLINE_STYLE UNIX)
    file(REMOVE "${native_path}")
endfunction()

set(generated_package "${evidence_root}/cfu_f_v4_reference.cxc")
run_checked(
    "canonical CXC pack"
    "${evidence_root}/cxc_pack.stdout.txt"
    "${evidence_root}/cxc_pack.stderr.txt"
    "${CUEXIS_CXC_PACK}" --input "${reference_project}" --output "${generated_package}")
assert_same_file("${generated_package}" "${golden_package}" "canonical CXC golden")

set(migrated_chart "${evidence_root}/chart_v4_static_migration.canonical.json")
set(migration_report "${evidence_root}/chart_v4_static_migration.report.json")
run_checked(
    "Chart v3 to v4 migration"
    "${evidence_root}/chart_migrator.stdout.txt"
    "${evidence_root}/chart_migrator.stderr.txt"
    "${CUEXIS_CHART_MIGRATOR}"
    --input "${migration_source}"
    --output "${migrated_chart}"
    --report "${migration_report}"
    --target 4)
assert_same_file("${migrated_chart}" "${golden_migration}" "Chart v4 migration golden")

run_checked(
    "headless semantic observation"
    "${evidence_root}/headless_consumer.stdout.txt"
    "${evidence_root}/headless_consumer.stderr.txt"
    "${CUEXIS_HEADLESS_CONSUMER}")
set(consumer_output "${CFU_F3_LAST_STDOUT}")

string(REGEX MATCH "identity=([0-9a-f]+)" identity_match "${consumer_output}")
set(semantic_identity "${CMAKE_MATCH_1}")
string(REGEX MATCH "stop_digest=([0-9]+)" digest_match "${consumer_output}")
set(stop_frame_digest "${CMAKE_MATCH_1}")
string(REGEX MATCH "diagnostics=([^\r\n]+)" diagnostics_match "${consumer_output}")
set(diagnostic_signature "${CMAKE_MATCH_1}")
if(identity_match STREQUAL "" OR digest_match STREQUAL "" OR diagnostics_match STREQUAL "")
    message(FATAL_ERROR
        "Headless consumer omitted a deterministic observation:\n${consumer_output}")
endif()

file(SHA256 "${generated_package}" cxc_sha256)
file(SIZE "${generated_package}" cxc_bytes)
file(SHA256 "${migrated_chart}" migration_chart_sha256)
file(SHA256 "${migration_report}" migration_report_sha256)

string(CONCAT fingerprint
    "format=cuexis.cfu-f3.determinism\n"
    "version=1\n"
    "cxc_sha256=${cxc_sha256}\n"
    "cxc_bytes=${cxc_bytes}\n"
    "semantic_identity=${semantic_identity}\n"
    "stop_frame_digest_v3=${stop_frame_digest}\n"
    "migration_chart_sha256=${migration_chart_sha256}\n"
    "migration_report_sha256=${migration_report_sha256}\n"
    "diagnostic_signature=${diagnostic_signature}\n")

write_lf("${evidence_root}/fingerprint.txt" "${fingerprint}")
file(SHA256 "${evidence_root}/fingerprint.txt" fingerprint_sha256)
string(CONCAT evidence
    "implementation_sha=${implementation_sha}\n"
    "fingerprint_sha256=${fingerprint_sha256}\n"
    "${fingerprint}")
write_lf("${evidence_root}/evidence.txt" "${evidence}")
configure_file("${golden_fingerprint}" "${evidence_root}/fingerprint.expected.txt" COPYONLY)

file(READ "${golden_fingerprint}" expected_fingerprint)
if(NOT fingerprint STREQUAL expected_fingerprint)
    message(FATAL_ERROR
        "CFU-F3 deterministic fingerprint differs from the committed golden.\n"
        "Expected:\n${expected_fingerprint}\n"
        "Actual:\n${fingerprint}")
endif()

message(STATUS
    "CFU-F3 deterministic fingerprint passed for implementation ${implementation_sha}")
