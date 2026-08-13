if(NOT DEFINED CUEXIS_CXC_PACK OR NOT DEFINED CUEXIS_CXC_VALIDATE OR
   NOT DEFINED CUEXIS_CXC_UNPACK OR NOT DEFINED CUEXIS_SOURCE_DIR OR
   NOT DEFINED CUEXIS_BINARY_DIR)
    message(FATAL_ERROR "CXC tool verification requires tool, source, and binary paths")
endif()

set(source_root
    "${CUEXIS_SOURCE_DIR}/tests/fixtures/chart_format_update/source_project")
set(golden_package
    "${CUEXIS_SOURCE_DIR}/tests/fixtures/chart_format_update/golden/cxc_v1_v4_cxt.cxc")
set(noncanonical_package
    "${CUEXIS_SOURCE_DIR}/tests/fixtures/chart_format_update/binary/valid_noncanonical_metadata.cxc")
set(invalid_package
    "${CUEXIS_SOURCE_DIR}/tests/fixtures/chart_format_update/binary/invalid_crc.cxc")
set(test_root "${CUEXIS_BINARY_DIR}/cxc-tool-test")
file(REMOVE_RECURSE "${test_root}")
file(MAKE_DIRECTORY "${test_root}")

function(assert_result actual expected label output error)
    if(NOT "${actual}" STREQUAL "${expected}")
        message(FATAL_ERROR
            "${label} returned ${actual}, expected ${expected}.\nstdout:\n${output}\nstderr:\n${error}")
    endif()
endfunction()

function(assert_no_host_paths label output error)
    string(REPLACE "\\" "/" normalized_output "${output}")
    string(REPLACE "\\" "/" normalized_error "${error}")
    string(REPLACE "\\" "/" normalized_source_dir "${CUEXIS_SOURCE_DIR}")
    string(REPLACE "\\" "/" normalized_binary_dir "${CUEXIS_BINARY_DIR}")
    if(WIN32)
        string(TOLOWER "${normalized_output}" normalized_output)
        string(TOLOWER "${normalized_error}" normalized_error)
        string(TOLOWER "${normalized_source_dir}" normalized_source_dir)
        string(TOLOWER "${normalized_binary_dir}" normalized_binary_dir)
    endif()
    if(NOT "${normalized_output}" STREQUAL "")
        string(FIND "${normalized_output}" "${normalized_source_dir}" source_position)
        string(FIND "${normalized_output}" "${normalized_binary_dir}" binary_position)
        if(NOT source_position EQUAL -1 OR NOT binary_position EQUAL -1)
            message(FATAL_ERROR "${label} stdout leaked a host path:\n${output}")
        endif()
    endif()
    if(NOT "${normalized_error}" STREQUAL "")
        string(FIND "${normalized_error}" "${normalized_source_dir}" source_position)
        string(FIND "${normalized_error}" "${normalized_binary_dir}" binary_position)
        if(NOT source_position EQUAL -1 OR NOT binary_position EQUAL -1)
            message(FATAL_ERROR "${label} stderr leaked a host path:\n${error}")
        endif()
    endif()
endfunction()

function(assert_same_file left right label)
    if(NOT EXISTS "${left}" OR NOT EXISTS "${right}")
        message(FATAL_ERROR "${label}: one of the compared files does not exist")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files "${left}" "${right}"
        RESULT_VARIABLE compare_result
    )
    if(NOT "${compare_result}" STREQUAL "0")
        message(FATAL_ERROR "${label}: files differ")
    endif()
endfunction()

function(assert_empty_directory path label)
    if(NOT IS_DIRECTORY "${path}")
        message(FATAL_ERROR "${label}: directory does not exist")
    endif()
    file(GLOB children "${path}/*")
    if(children)
        message(FATAL_ERROR "${label}: directory is not empty")
    endif()
endfunction()

set(packed_package "${test_root}/packed.cxc")
execute_process(
    COMMAND "${CUEXIS_CXC_PACK}" --input "${source_root}" --output "${packed_package}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
assert_result("${result}" "0" "pack canonical source" "${output}" "${error}")
if(NOT "${error}" STREQUAL "")
    message(FATAL_ERROR "pack canonical source wrote diagnostics to stderr:\n${error}")
endif()
assert_no_host_paths("pack canonical source" "${output}" "${error}")
assert_same_file("${packed_package}" "${golden_package}" "canonical pack golden")

file(GLOB before_validate RELATIVE "${test_root}" "${test_root}/*")
execute_process(
    COMMAND "${CUEXIS_CXC_VALIDATE}" --input "${packed_package}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
assert_result("${result}" "0" "validate canonical package" "${output}" "${error}")
assert_no_host_paths("validate canonical package" "${output}" "${error}")
file(GLOB after_validate RELATIVE "${test_root}" "${test_root}/*")
list(SORT before_validate)
list(SORT after_validate)
if(NOT "${before_validate}" STREQUAL "${after_validate}")
    message(FATAL_ERROR "validate changed files in its working directory")
endif()

execute_process(
    COMMAND "${CUEXIS_CXC_VALIDATE}" --input "${invalid_package}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
assert_result("${result}" "1" "validate invalid package" "${output}" "${error}")
if(NOT "${output}" STREQUAL "")
    message(FATAL_ERROR "validate invalid package wrote a success summary")
endif()
if(NOT "${error}" MATCHES "cxc.archive.invalid")
    message(FATAL_ERROR "validate invalid package omitted its stable diagnostic:\n${error}")
endif()
assert_no_host_paths("validate invalid package" "${output}" "${error}")

execute_process(
    COMMAND "${CUEXIS_CXC_VALIDATE}" --input "${test_root}/missing.cxc"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
assert_result("${result}" "2" "validate missing package" "${output}" "${error}")
assert_no_host_paths("validate missing package" "${output}" "${error}")

execute_process(
    COMMAND "${CUEXIS_CXC_VALIDATE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
assert_result("${result}" "2" "validate argument error" "${output}" "${error}")
assert_no_host_paths("validate argument error" "${output}" "${error}")

set(conflict_output "${source_root}/conflict.cxc")
file(REMOVE "${conflict_output}")
execute_process(
    COMMAND "${CUEXIS_CXC_PACK}" --input "${source_root}" --output "${conflict_output}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
assert_result("${result}" "2" "pack input/output conflict" "${output}" "${error}")
if(EXISTS "${conflict_output}")
    message(FATAL_ERROR "pack input/output conflict created an output file")
endif()
assert_no_host_paths("pack input/output conflict" "${output}" "${error}")

set(invalid_project_root "${test_root}/invalid-project")
file(MAKE_DIRECTORY "${invalid_project_root}")
file(COPY "${source_root}/" DESTINATION "${invalid_project_root}")
file(READ "${invalid_project_root}/cuexis.project.json" invalid_project)
string(REPLACE "\"root\": \"main\"" "\"root\": \"missing\"" invalid_project
               "${invalid_project}")
file(WRITE "${invalid_project_root}/cuexis.project.json" "${invalid_project}")
execute_process(
    COMMAND "${CUEXIS_CXC_PACK}" --input "${invalid_project_root}" --output
            "${test_root}/invalid-project.cxc"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
assert_result("${result}" "1" "pack invalid project" "${output}" "${error}")
if(NOT "${output}" STREQUAL "" OR NOT "${error}" MATCHES "project.entry.root_missing")
    message(FATAL_ERROR "pack invalid project omitted its stable diagnostic:\n${error}")
endif()
assert_no_host_paths("pack invalid project" "${output}" "${error}")

set(linked_source_root "${test_root}/linked-source")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E create_symlink "${source_root}" "${linked_source_root}"
    RESULT_VARIABLE link_result
    OUTPUT_QUIET
    ERROR_QUIET
)
if("${link_result}" STREQUAL "0")
    execute_process(
        COMMAND "${CUEXIS_CXC_PACK}" --input "${linked_source_root}" --output
                "${test_root}/linked-source.cxc"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    assert_result("${result}" "2" "pack linked source root" "${output}" "${error}")
    if(NOT "${error}" MATCHES "cxc.pack.source_root_unavailable")
        message(FATAL_ERROR "pack linked source root omitted its stable diagnostic:\n${error}")
    endif()
    assert_no_host_paths("pack linked source root" "${output}" "${error}")
endif()

set(nonempty_target "${test_root}/nonempty")
file(MAKE_DIRECTORY "${nonempty_target}")
file(WRITE "${nonempty_target}/sentinel.txt" "preserve\n")
execute_process(
    COMMAND "${CUEXIS_CXC_UNPACK}" --input "${packed_package}" --output "${nonempty_target}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
assert_result("${result}" "2" "unpack non-empty target" "${output}" "${error}")
file(READ "${nonempty_target}/sentinel.txt" sentinel)
if(NOT "${sentinel}" STREQUAL "preserve\n")
    message(FATAL_ERROR "unpack non-empty target modified existing content")
endif()
assert_no_host_paths("unpack non-empty target" "${output}" "${error}")

set(unpacked_target "${test_root}/unpacked")
execute_process(
    COMMAND "${CUEXIS_CXC_UNPACK}" --input "${packed_package}" --output "${unpacked_target}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
assert_result("${result}" "0" "unpack canonical package" "${output}" "${error}")
assert_no_host_paths("unpack canonical package" "${output}" "${error}")

set(repacked_package "${test_root}/repacked.cxc")
execute_process(
    COMMAND "${CUEXIS_CXC_PACK}" --input "${unpacked_target}" --output "${repacked_package}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
assert_result("${result}" "0" "repack canonical package" "${output}" "${error}")
assert_no_host_paths("repack canonical package" "${output}" "${error}")
assert_same_file("${repacked_package}" "${golden_package}" "canonical round trip")

set(noncanonical_unpacked "${test_root}/noncanonical-unpacked")
execute_process(
    COMMAND "${CUEXIS_CXC_UNPACK}" --input "${noncanonical_package}" --output
            "${noncanonical_unpacked}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
assert_result("${result}" "0" "unpack valid noncanonical package" "${output}" "${error}")
assert_no_host_paths("unpack valid noncanonical package" "${output}" "${error}")
set(noncanonical_repacked "${test_root}/noncanonical-repacked.cxc")
execute_process(
    COMMAND "${CUEXIS_CXC_PACK}" --input "${noncanonical_unpacked}" --output
            "${noncanonical_repacked}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
assert_result("${result}" "0" "repack noncanonical package" "${output}" "${error}")
assert_no_host_paths("repack noncanonical package" "${output}" "${error}")
assert_same_file("${noncanonical_repacked}" "${golden_package}"
                 "noncanonical input canonicalization")

set(staging_failure_target "${test_root}/staging-failure")
file(MAKE_DIRECTORY "${staging_failure_target}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env CUEXIS_CXC_UNPACK_FAIL_AFTER_ENTRY=1
            "${CUEXIS_CXC_UNPACK}" --input "${packed_package}" --output
            "${staging_failure_target}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
assert_result("${result}" "2" "unpack staging failure" "${output}" "${error}")
assert_no_host_paths("unpack staging failure" "${output}" "${error}")
assert_empty_directory("${staging_failure_target}" "unpack staging failure rollback")
file(GLOB staging_leftovers "${test_root}/staging-failure.cuexis-unpack-staging-*")
if(staging_leftovers)
    message(FATAL_ERROR "unpack staging failure left staging directories")
endif()

set(commit_failure_target "${test_root}/commit-failure")
file(MAKE_DIRECTORY "${commit_failure_target}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env CUEXIS_CXC_UNPACK_FAIL_AFTER_BACKUP=1
            "${CUEXIS_CXC_UNPACK}" --input "${packed_package}" --output
            "${commit_failure_target}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
assert_result("${result}" "2" "unpack commit failure" "${output}" "${error}")
assert_no_host_paths("unpack commit failure" "${output}" "${error}")
assert_empty_directory("${commit_failure_target}" "unpack commit failure rollback")
file(GLOB backup_leftovers "${test_root}/commit-failure.cuexis-unpack-backup-*")
if(backup_leftovers)
    message(FATAL_ERROR "unpack commit failure left backup directories")
endif()

set(unpack_restore_failure_target "${test_root}/unpack-restore-failure")
file(MAKE_DIRECTORY "${unpack_restore_failure_target}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env CUEXIS_CXC_UNPACK_FAIL_AFTER_BACKUP=1
            CUEXIS_CXC_UNPACK_FAIL_RESTORE=1 "${CUEXIS_CXC_UNPACK}" --input
            "${packed_package}" --output "${unpack_restore_failure_target}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
assert_result("${result}" "2" "unpack restore failure" "${output}" "${error}")
if(NOT "${error}" MATCHES "cxc.unpack.output_restore_failed")
    message(FATAL_ERROR "unpack restore failure omitted its stable diagnostic:\n${error}")
endif()
assert_no_host_paths("unpack restore failure" "${output}" "${error}")
if(EXISTS "${unpack_restore_failure_target}")
    message(FATAL_ERROR "unpack restore failure recreated the output unexpectedly")
endif()
file(GLOB unpack_preserved_backups
     "${test_root}/unpack-restore-failure.cuexis-unpack-backup-*")
list(LENGTH unpack_preserved_backups unpack_preserved_backup_count)
if(NOT unpack_preserved_backup_count EQUAL 1)
    message(FATAL_ERROR "unpack restore failure did not preserve exactly one backup")
endif()
list(GET unpack_preserved_backups 0 unpack_preserved_backup)
assert_empty_directory("${unpack_preserved_backup}" "unpack restore failure backup")
file(REMOVE_RECURSE "${unpack_preserved_backup}")

set(pack_commit_failure "${test_root}/pack-commit-failure.cxc")
file(COPY_FILE "${golden_package}" "${pack_commit_failure}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env CUEXIS_CXC_PACK_FAIL_AFTER_BACKUP=1
            "${CUEXIS_CXC_PACK}" --input "${source_root}" --output
            "${pack_commit_failure}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
assert_result("${result}" "2" "pack commit failure" "${output}" "${error}")
assert_no_host_paths("pack commit failure" "${output}" "${error}")
assert_same_file("${pack_commit_failure}" "${golden_package}" "pack commit failure rollback")
file(GLOB pack_backup_leftovers "${test_root}/pack-commit-failure.cuexis-pack-backup-*")
if(pack_backup_leftovers)
    message(FATAL_ERROR "pack commit failure left backup files")
endif()

set(pack_restore_failure "${test_root}/pack-restore-failure.cxc")
file(COPY_FILE "${golden_package}" "${pack_restore_failure}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env CUEXIS_CXC_PACK_FAIL_AFTER_BACKUP=1
            CUEXIS_CXC_PACK_FAIL_RESTORE=1 "${CUEXIS_CXC_PACK}" --input "${source_root}"
            --output "${pack_restore_failure}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
assert_result("${result}" "2" "pack restore failure" "${output}" "${error}")
if(NOT "${error}" MATCHES "cxc.pack.output_restore_failed")
    message(FATAL_ERROR "pack restore failure omitted its stable diagnostic:\n${error}")
endif()
assert_no_host_paths("pack restore failure" "${output}" "${error}")
if(EXISTS "${pack_restore_failure}")
    message(FATAL_ERROR "pack restore failure recreated the output unexpectedly")
endif()
file(GLOB pack_preserved_backups
     "${test_root}/pack-restore-failure.cxc.cuexis-pack-backup-*.cxc")
list(LENGTH pack_preserved_backups pack_preserved_backup_count)
if(NOT pack_preserved_backup_count EQUAL 1)
    message(FATAL_ERROR "pack restore failure did not preserve exactly one backup")
endif()
list(GET pack_preserved_backups 0 pack_preserved_backup)
assert_same_file("${pack_preserved_backup}" "${golden_package}" "pack restore failure backup")
file(REMOVE "${pack_preserved_backup}")

file(REMOVE_RECURSE "${test_root}")
