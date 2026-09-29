cmake_minimum_required(VERSION 3.25)
# Independent test for the reference host command case parser.
#
# Why this exists
# ---------------
# The command gate is itself the only judge of the command cases, so a defect in
# the gate cannot be observed from the gate's own success count. Three defects of
# exactly that kind reached the hosted runners before this file did.
#
# What makes it independent
# -------------------------
# It reads the same declaration data file the gate reads --
# ReferenceHostCommandCases.txt -- but validates it with a different algorithm.
# The gate counts '|' separators, indexes the split fields, and extracts the flags
# field with a separate pattern. This script matches each declaration against one
# anchored pattern with capture groups. Sharing the data is not sharing the
# judgement: a change that breaks the gate's algorithm without changing the data
# is invisible to the gate's own counting and visible here.
#
# The samples below are written here rather than generated from the data file. If
# they were generated from it they would agree with it by construction.

set(cuexis_parser_failed FALSE)

# Two helpers rather than one condition-taking helper. A helper that took a
# condition as text and handed it to if() would expand a text condition to a
# constant that is true for any word CMake does not treat as false, so
# `if(NOT EXISTS)` written as a bare word would quietly pass. Passing a boolean,
# or a real comparison performed inside the helper, keeps every check honest.
function(cuexis_parser_check_true is_ok message)
    if(NOT is_ok)
        message(STATUS "  FAIL ${message}")
        set(cuexis_parser_failed TRUE PARENT_SCOPE)
    endif()
endfunction()

function(cuexis_parser_check_eq actual expected message)
    if(NOT "${actual}" STREQUAL "${expected}")
        message(STATUS "  FAIL ${message}")
        set(cuexis_parser_failed TRUE PARENT_SCOPE)
    endif()
endfunction()

# ---------------------------------------------------------------------------
# The independent algorithm. One anchored match, capture groups, no indexing.
# ---------------------------------------------------------------------------
set(cuexis_parser_id_pattern "[a-z][a-z0-9-]*")
set(cuexis_parser_kind_pattern "cmd|invocation|generated|legacy")
set(cuexis_parser_flag_pattern "[a-z][a-z0-9-]*")
set(cuexis_parser_declaration_pattern
    "^(${cuexis_parser_id_pattern})\\|(${cuexis_parser_kind_pattern})\\|(${cuexis_parser_flag_pattern})?$")

function(cuexis_parser_classify declaration verdict_out id_out kind_out flags_out)
    if(declaration MATCHES "${cuexis_parser_declaration_pattern}")
        set(${verdict_out} "accept" PARENT_SCOPE)
        set(${id_out} "${CMAKE_MATCH_1}" PARENT_SCOPE)
        set(${kind_out} "${CMAKE_MATCH_2}" PARENT_SCOPE)
        set(${flags_out} "${CMAKE_MATCH_3}" PARENT_SCOPE)
    else()
        set(${verdict_out} "reject" PARENT_SCOPE)
        set(${id_out} "" PARENT_SCOPE)
        set(${kind_out} "" PARENT_SCOPE)
        set(${flags_out} "" PARENT_SCOPE)
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Verification section 1: the policy baseline required by D2 is in effect, and
# the empty-element detector required by D11 is therefore live.
#
# list(FIND <list> "" <out>) only detects an empty element while policy CMP0007 is
# NEW; under the OLD behaviour list() drops empty elements and the search reports
# a clean list. So this assertion is only meaningful once the entry point has
# declared the baseline -- which is the point.
#
# Scope, measured rather than assumed. On CMake 3.28.3 the check bites: without
# the declaration it reports index '-1' and length 2 and this script fails. On
# CMake 4.x it cannot bite, because 4.x has no OLD behaviour to select and the
# policies are NEW whether or not anything declares them -- verified by removing
# the declaration and watching the check still pass under 4.3.3. So a passing run
# here proves the detector is live on 3.x, and proves nothing about 3.x when it is
# run on 4.x. That asymmetry is why the lower-bound job pins an actual 3.25.x.
#
# IN_LIST is used in section 3 for the same reason: it is only an if() operator
# while CMP0057 is NEW, so these checks double as a test of the baseline.
# ---------------------------------------------------------------------------
set(cuexis_parser_dirty_list "a;;b")

list(FIND cuexis_parser_dirty_list "" cuexis_parser_empty_index)
cuexis_parser_check_eq("${cuexis_parser_empty_index}" "1"
    "the empty-element detector is not live: a list with an empty element reported index '${cuexis_parser_empty_index}', so policy CMP0007 is not NEW")

list(LENGTH cuexis_parser_dirty_list cuexis_parser_dirty_length)
cuexis_parser_check_eq("${cuexis_parser_dirty_length}" "3"
    "'a;;b' has length ${cuexis_parser_dirty_length}, not 3, so empty elements are still being dropped")

# The audit uses a local policy override rather than the entry point's baseline.
# This asserts that the push/set/pop isolation it relies on works.
cmake_policy(PUSH)
cmake_policy(SET CMP0007 NEW)
list(LENGTH cuexis_parser_dirty_list cuexis_parser_isolated_length)
cmake_policy(POP)
cuexis_parser_check_eq("${cuexis_parser_isolated_length}" "3"
    "a local cmake_policy(PUSH/SET CMP0007 NEW/POP) block did not make the detector live")

# ---------------------------------------------------------------------------
# Verification section 2: hand-written samples covering the declaration shapes.
# Each entry is <expected verdict>@@<declaration>. The escaped ';' in one sample
# is CMake list syntax; the parser sees an unescaped one.
# ---------------------------------------------------------------------------
set(cuexis_parser_samples
    "accept@@c01-absolute-anchor|cmd|"
    "accept@@c02-pause-resume|cmd|seek"
    "accept@@c12-legacy-regression|legacy|"
    "accept@@n07a-missing-file|invocation|missing-file"
    "accept@@n05a-file-too-large|generated|"
    "reject@@bad|cmd"
    "reject@@bad|cmd|seek|extra"
    "reject@@|cmd|"
    "reject@@bad||"
    "reject@@bad|nosuchkind|"
    "reject@@BAD-ID|cmd|"
    "reject@@bad-id|CMD|"
    "reject@@bad-id|cmd|Seek"
    "reject@@bad-id|cmd|trailing space"
    "reject@@bad_id|cmd|"
)

foreach(cuexis_parser_sample IN LISTS cuexis_parser_samples)
    string(REPLACE "@@" ";" cuexis_parser_sample_parts "${cuexis_parser_sample}")
    list(GET cuexis_parser_sample_parts 0 cuexis_parser_want)
    list(GET cuexis_parser_sample_parts 1 cuexis_parser_declaration)
    cuexis_parser_classify("${cuexis_parser_declaration}"
        cuexis_parser_verdict cuexis_parser_id cuexis_parser_kind cuexis_parser_flags)
    cuexis_parser_check_eq("${cuexis_parser_verdict}" "${cuexis_parser_want}"
        "'${cuexis_parser_declaration}' was ${cuexis_parser_verdict}, want ${cuexis_parser_want}")
endforeach()

# A declaration whose flags field carries a ';' must be rejected. It is checked
# outside the sample list above because a ';' cannot survive inside a CMake list
# element: splitting the sample on its own separator would truncate it first, and
# the check would then be testing the truncated declaration instead.
set(cuexis_parser_semicolon_declaration "bad-id|cmd|a;b")
cuexis_parser_classify("${cuexis_parser_semicolon_declaration}"
    cuexis_parser_verdict cuexis_parser_id cuexis_parser_kind cuexis_parser_flags)
cuexis_parser_check_eq("${cuexis_parser_verdict}" "reject"
    "a declaration whose flags field carries a ';' was accepted")

# The two accept samples that carry a flags field must round-trip it exactly.
cuexis_parser_classify("c02-pause-resume|cmd|seek"
    cuexis_parser_verdict cuexis_parser_id cuexis_parser_kind cuexis_parser_flags)
cuexis_parser_check_eq("${cuexis_parser_flags}" "seek"
    "the flags field of 'c02-pause-resume|cmd|seek' came back as '${cuexis_parser_flags}'")
cuexis_parser_classify("c02-pause-resume|cmd|"
    cuexis_parser_verdict cuexis_parser_id cuexis_parser_kind cuexis_parser_flags)
cuexis_parser_check_eq("${cuexis_parser_flags}" ""
    "an empty flags field came back as '${cuexis_parser_flags}' rather than empty")

# ---------------------------------------------------------------------------
# Verification section 3: every real declaration must satisfy the independent
# algorithm. This is the part that catches the data drifting away from the
# grammar, and the only part that reads the shipped data.
# ---------------------------------------------------------------------------
set(cuexis_parser_data_file "${CMAKE_CURRENT_LIST_DIR}/ReferenceHostCommandCases.txt")
if(EXISTS "${cuexis_parser_data_file}")
    set(cuexis_parser_data_present TRUE)
else()
    set(cuexis_parser_data_present FALSE)
endif()
cuexis_parser_check_true("${cuexis_parser_data_present}"
    "the command case data file is missing: ${cuexis_parser_data_file}")

file(READ "${cuexis_parser_data_file}" cuexis_parser_data_text)
string(REPLACE "\r\n" "\n" cuexis_parser_data_text "${cuexis_parser_data_text}")
# Escaped before the split and restored per line for the same reason the gate
# does it: a ';' anywhere, including in a comment, would split a list element.
string(REPLACE ";" "\\;" cuexis_parser_data_text "${cuexis_parser_data_text}")
string(REPLACE "\n" ";" cuexis_parser_data_lines "${cuexis_parser_data_text}")

set(cuexis_parser_seen_ids "")
set(cuexis_parser_declaration_count 0)
foreach(cuexis_parser_line IN LISTS cuexis_parser_data_lines)
    string(REPLACE "\\;" ";" cuexis_parser_line "${cuexis_parser_line}")
    string(STRIP "${cuexis_parser_line}" cuexis_parser_line)
    if(cuexis_parser_line STREQUAL "" OR cuexis_parser_line MATCHES "^#")
        continue()
    endif()
    math(EXPR cuexis_parser_declaration_count "${cuexis_parser_declaration_count} + 1")
    cuexis_parser_classify("${cuexis_parser_line}"
        cuexis_parser_verdict cuexis_parser_id cuexis_parser_kind cuexis_parser_flags)
    cuexis_parser_check_eq("${cuexis_parser_verdict}" "accept"
        "the data file declares '${cuexis_parser_line}', which the independent grammar rejects")
    if(cuexis_parser_id IN_LIST cuexis_parser_seen_ids)
        cuexis_parser_check_true(FALSE
            "the data file declares '${cuexis_parser_id}' more than once")
    endif()
    list(APPEND cuexis_parser_seen_ids "${cuexis_parser_id}")
endforeach()

if(cuexis_parser_declaration_count GREATER 0)
    set(cuexis_parser_count_ok TRUE)
else()
    set(cuexis_parser_count_ok FALSE)
endif()
cuexis_parser_check_true("${cuexis_parser_count_ok}"
    "the data file declares no cases at all")

# Every declaration must round-trip its id through the independent algorithm
# unchanged, so that a grammar which accepts a line but loses part of it is caught
# rather than counted.
set(cuexis_parser_roundtrip_index 0)
foreach(cuexis_parser_seen_id IN LISTS cuexis_parser_seen_ids)
    cuexis_parser_classify("${cuexis_parser_seen_id}|cmd|"
        cuexis_parser_verdict cuexis_parser_id cuexis_parser_kind cuexis_parser_flags)
    cuexis_parser_check_eq("${cuexis_parser_id}" "${cuexis_parser_seen_id}"
        "the id '${cuexis_parser_seen_id}' round-tripped as '${cuexis_parser_id}'")
    math(EXPR cuexis_parser_roundtrip_index "${cuexis_parser_roundtrip_index} + 1")
endforeach()
cuexis_parser_check_eq("${cuexis_parser_roundtrip_index}" "${cuexis_parser_declaration_count}"
    "round-tripped ${cuexis_parser_roundtrip_index} ids but read ${cuexis_parser_declaration_count} declarations")

# ---------------------------------------------------------------------------
if(cuexis_parser_failed)
    message(FATAL_ERROR
        "Reference host command parser test failed; see the FAIL lines above")
endif()
message(STATUS
    "Reference host command parser test passed: samples, detector liveness and ${cuexis_parser_declaration_count} declarations")
