# R9 command-mode case runner for the reference host.
#
# Included by VerifyReferenceHost.cmake after the legacy run and its golden
# assertions have passed. It drives the same host executable once per declared
# case with an external command file and checks the run record field by field.
#
# The mandatory case list below is the declaration of record. The fixtures
# under the copied example tree are cross-checked against it for set equality;
# glob is used only as validation, never as discovery, so a fixture that exists
# but was never declared fails instead of being skipped silently.
#
# Required incoming variables:
#   host_executable     - the built reference host
#   command_fixture_dir - the copied example tree's tests/commands directory
#   host_build          - the directory holding the host's runtime libraries
#   work_dir            - the staging work directory
#   content_dir         - the staged reference content root's parent
#   package_path        - the staged .cxc package
#   golden_identity     - the frozen CFU-F reference identity
#   golden_digest       - the frozen reference digest, spelled "<index>=<value>"
#   report_path         - the legacy run record (used by the legacy case)
#   clean_path          - the sanitized PATH for the child process

set(cuexis_command_fixture_dir "${command_fixture_dir}")
set(cuexis_command_output_dir "${work_dir}/command-cases")
set(cuexis_command_space_root "${work_dir}/content with space/cfu_f_reference_project")
set(cuexis_command_content "${content_dir}/cfu_f_reference_project")

# The frozen digest value, recovered from the "<index>=<value>" spelling that
# the legacy block already uses. The case runner must never carry a second copy
# of this number: the whole point of the anchor rule is that there is exactly
# one source for it.
if(NOT golden_digest MATCHES "^[0-9]+=([0-9]+)$")
    message(FATAL_ERROR "The golden digest is not spelled <index>=<value>: '${golden_digest}'")
endif()
set(cuexis_golden_digest_value "${CMAKE_MATCH_1}")

# ---------------------------------------------------------------------------
# The declared case list. Each entry is "<case-id>|<kind>|<flags>".
#
# kind  cmd        - fixture-backed: needs <id>.cmd and <id>.expect
#       invocation - no fixture; the runner constructs the invocation itself
#       generated  - no fixture; the runner writes the input at run time
#       legacy     - C12; its evidence is the pre-existing legacy run above
#
# flags select extra argv for the case. They exist because several negative
# cases are about the flag surface rather than about file content.
# ---------------------------------------------------------------------------
set(cuexis_command_cases
    # --- positive: transport, clock and digest behaviour ---
    "c01-absolute-anchor|cmd|"
    "c02-pause-resume|cmd|"
    "c03-continuous-control|cmd|"
    "c04-paused-reload-twice|cmd|"
    "c05-playing-reload|cmd|"
    "c06-ready-suppression|cmd|"
    "c07-paused-seek|cmd|"
    "c08-idempotence|cmd|"
    "c09-zero-frame-play|cmd|"
    "c10a-empty-quit|cmd|"
    "c10b-empty-quit-expect-identity|cmd|expect-identity"
    "c11a-space-path|cmd|"
    "c11b-line-endings-bom|cmd|"
    # --- C12: the legacy invocation and its golden assertions, already run ---
    "c12-legacy-regression|legacy|"
    # --- negative: pre-open verbs ---
    "n01a-play-before-open|cmd|"
    "n01b-pause-before-open|cmd|"
    "n01c-tick-before-open|cmd|"
    "n01d-seek-before-open|cmd|"
    "n01e-reload-before-open|cmd|"
    # --- negative: reload without a sample ---
    "n02a-reload-no-sample|cmd|"
    "n02b-play-reload-no-sample|cmd|"
    # --- negative: second open ---
    "n03-second-open|cmd|"
    # --- negative: malformed programs ---
    "n04a-unknown-verb|cmd|"
    "n04b-trailing-arg|cmd|"
    "n04c-missing-arg|cmd|"
    "n04d-bad-number|cmd|"
    "n04e-no-quit|cmd|"
    "n04f-after-quit|cmd|"
    # --- negative: budgets ---
    "n05a-file-too-large|generated|"
    "n05b-too-many-commands|generated|"
    "n05c-tick-budget-ok|cmd|"
    "n05d-tick-budget-exceeded|cmd|"
    "n05e-seek-out-of-range|cmd|"
    "n05f-seek-then-tick-overflow|cmd|"
    "n05g-integer-overflow|cmd|"
    # --- negative: flag surface ---
    "n06a-advance-conflict|cmd|advance"
    "n06b-package-conflict|cmd|package"
    "n06c-expect-digest-conflict|cmd|expect-digest"
    "n06d-duplicate-command-file|cmd|duplicate-command-file"
    # --- negative: input and IO ---
    "n07a-missing-file|invocation|missing-file"
    "n07b-wrong-content-root|cmd|wrong-content-root")

set(cuexis_command_case_ids "")
set(cuexis_command_fixture_ids "")
set(cuexis_command_expectation_ids "")
foreach(entry IN LISTS cuexis_command_cases)
    # The declaration is <id>|<kind>|<flags> and the flags field is legitimately
    # empty for most cases. The shape is checked by counting separators rather
    # than by splitting, because splitting leaves a trailing empty element that
    # not every CMake version keeps: the same declaration was well formed with a
    # trailing element counted and malformed where it was dropped, so the gate
    # passed locally and failed on the runner. Counting also rejects a
    # declaration that is missing the field entirely.
    string(REGEX MATCHALL "\\|" separators "${entry}")
    list(LENGTH separators separator_count)
    if(NOT separator_count EQUAL 2)
        message(FATAL_ERROR "Malformed case declaration '${entry}': expected <id>|<kind>|<flags>")
    endif()
    string(REPLACE "|" ";" fields "${entry}")
    list(GET fields 0 case_id)
    list(GET fields 1 case_kind)
    if(NOT case_kind MATCHES "^(cmd|invocation|generated|legacy)$")
        message(FATAL_ERROR "Unknown case kind '${case_kind}' in '${entry}'")
    endif()
    # A .cmd fixture is required only for the cmd kind; a .expect file is
    # required for every kind that actually runs the host, so that a generated
    # or invocation case cannot pass by having nothing to assert.
    if(case_kind STREQUAL "cmd")
        list(APPEND cuexis_command_fixture_ids "${case_id}")
    endif()
    if(NOT case_kind STREQUAL "legacy")
        list(APPEND cuexis_command_expectation_ids "${case_id}")
    endif()
    list(APPEND cuexis_command_case_ids "${case_id}")
endforeach()

list(LENGTH cuexis_command_case_ids cuexis_command_expected_count)
if(cuexis_command_expected_count EQUAL 0)
    message(FATAL_ERROR "The command-mode case list is empty; an empty gate is not a passing gate")
endif()
list(FIND cuexis_command_case_ids "c12-legacy-regression" legacy_index)
if(legacy_index EQUAL -1)
    message(FATAL_ERROR "The command-mode case list no longer declares the legacy regression case")
endif()

# ---------------------------------------------------------------------------
# Set equality: declared cmd-kind cases against the fixtures on disk. Glob only
# validates; it never adds a case to the run.
# ---------------------------------------------------------------------------
if(NOT IS_DIRECTORY "${cuexis_command_fixture_dir}")
    message(FATAL_ERROR
        "The command fixture directory is missing: '${cuexis_command_fixture_dir}'. "
        "The example tree is copied whole, so this means the fixtures were never added.")
endif()
file(GLOB cuexis_command_cmd_files RELATIVE "${cuexis_command_fixture_dir}"
    "${cuexis_command_fixture_dir}/*.cmd")
file(GLOB cuexis_command_expect_files RELATIVE "${cuexis_command_fixture_dir}"
    "${cuexis_command_fixture_dir}/*.expect")
list(SORT cuexis_command_cmd_files)
list(SORT cuexis_command_expect_files)
list(SORT cuexis_command_fixture_ids)
list(SORT cuexis_command_expectation_ids)

set(cuexis_command_cmd_ids "")
foreach(name IN LISTS cuexis_command_cmd_files)
    string(REGEX REPLACE "\\.cmd$" "" stem "${name}")
    list(APPEND cuexis_command_cmd_ids "${stem}")
endforeach()
set(cuexis_command_expect_ids "")
foreach(name IN LISTS cuexis_command_expect_files)
    string(REGEX REPLACE "\\.expect$" "" stem "${name}")
    list(APPEND cuexis_command_expect_ids "${stem}")
endforeach()

if(NOT cuexis_command_cmd_ids STREQUAL cuexis_command_fixture_ids)
    message(FATAL_ERROR
        "Declared command cases and *.cmd fixtures disagree.\n"
        "  declared: ${cuexis_command_fixture_ids}\n"
        "  on disk:  ${cuexis_command_cmd_ids}")
endif()
if(NOT cuexis_command_expect_ids STREQUAL cuexis_command_expectation_ids)
    message(FATAL_ERROR
        "Declared command cases and *.expect files disagree.\n"
        "  declared: ${cuexis_command_expectation_ids}\n"
        "  on disk:  ${cuexis_command_expect_ids}")
endif()

# ---------------------------------------------------------------------------
# Run directories. The per-case directory is flat so that a fixture's relative
# open path has a stable depth: the sibling "content with space" root is what
# the input-portability case addresses.
# ---------------------------------------------------------------------------
file(REMOVE_RECURSE "${cuexis_command_output_dir}")
file(MAKE_DIRECTORY "${cuexis_command_output_dir}")
file(COPY "${cuexis_command_content}"
    DESTINATION "${work_dir}/content with space")

foreach(case_id IN LISTS cuexis_command_fixture_ids)
    file(COPY "${cuexis_command_fixture_dir}/${case_id}.cmd"
        DESTINATION "${cuexis_command_output_dir}")
endforeach()

# The oversized input is generated rather than committed: 1 MiB of comments is
# a budget boundary, not a document worth carrying in the repository. It is
# rejected on file size before any line is interpreted, so its content only has
# to be long enough.
set(oversized_file "${cuexis_command_output_dir}/n05a-file-too-large.cmd")
file(WRITE "${oversized_file}" "")
set(filler "# 0123456789012345678901234567890123456789012345678901234567890123456789012345678901\n")
foreach(index RANGE 1 13000)
    file(APPEND "${oversized_file}" "${filler}")
endforeach()
file(SIZE "${oversized_file}" oversized_size)
if(oversized_size LESS_EQUAL 1048576)
    message(FATAL_ERROR
        "The generated oversized command file is only ${oversized_size} bytes; "
        "the 1 MiB boundary would not be exercised")
endif()

# The over-long program is likewise generated. Each line is a real command so
# that the count, not the file size, is the boundary under test.
set(overlong_file "${cuexis_command_output_dir}/n05b-too-many-commands.cmd")
file(WRITE "${overlong_file}" "")
foreach(index RANGE 1 10001)
    file(APPEND "${overlong_file}" "play\n")
endforeach()
file(APPEND "${overlong_file}" "quit\n")

# ---------------------------------------------------------------------------
# Assertion helpers. Failures accumulate so that one case cannot hide the rest;
# the run fails once at the end with the full accounting.
# ---------------------------------------------------------------------------
set(cuexis_command_failures "")
set(cuexis_command_passed_ids "")
set(cuexis_command_completed_ids "")
set(cuexis_command_evidence "")

function(cuexis_command_record_failure case_id message)
    set(accumulated "${cuexis_command_failures}")
    list(APPEND accumulated "${case_id}: ${message}")
    set(cuexis_command_failures "${accumulated}" PARENT_SCOPE)
endfunction()

# There is deliberately no "assert(condition_as_text)" helper here. A condition
# assembled as a string and expanded into if(NOT ${condition}) is re-tokenized on
# whitespace, so any quotes inside it stop being quotes and the comparison can
# silently pass. Every assertion below is a real if() so its quoting is fixed
# when the file is parsed.

# An expected substring is matched literally: the fixtures are plain text and
# must not be able to smuggle a regular expression into the gate.
#
# The backslash is escaped first. Doing it second would re-escape the backslash
# just inserted for the dot, turning a literal "." into "\\.", which matches a
# backslash followed by any character instead of a dot.
function(cuexis_command_escape_literal text out_var)
    string(REPLACE "\\" "\\\\" escaped "${text}")
    string(REPLACE "." "\\." escaped "${escaped}")
    set(${out_var} "${escaped}" PARENT_SCOPE)
endfunction()

# Rejoin tokens back into one literal.
#
# separate_arguments() splits an .expect line on spaces, but a required literal
# is a whole report fragment such as
#   require host.command index=1 line=3 verb=seek outcome=ok
# Taking only the token after the directive would compare the record against
# "host.command" and pass for any command line whatsoever, which is the same
# class of vacuous check section 8.6 forbids.
function(cuexis_command_join_range parts first last out_var)
    set(joined "")
    if(NOT first GREATER last)
        foreach(index RANGE ${first} ${last})
            list(GET parts ${index} part)
            if(joined STREQUAL "")
                set(joined "${part}")
            else()
                set(joined "${joined} ${part}")
            endif()
        endforeach()
    endif()
    set(${out_var} "${joined}" PARENT_SCOPE)
endfunction()

# The frozen .expect grammar from R9 section 3.6.2. A directive outside this set
# is a fixture defect, not a case failure to be attributed to the host.
set(cuexis_command_known_directives
    exit code diagnostic-step frame digest-anchor
    count require require-count absent)

# ---------------------------------------------------------------------------
# Pass 1: a case whose input is a fixture that the contract requires to be
# rejected before any command is dispatched. Section 8.5 rejects "any crash" as
# negative evidence, so the exit code, the diagnostic code, the diagnostic step
# and the absence of SDK activity are all asserted.
# ---------------------------------------------------------------------------
set(cuexis_command_expected_ids "${cuexis_command_case_ids}")
set(cuexis_command_case_total 0)

foreach(entry IN LISTS cuexis_command_cases)
    string(REPLACE "|" ";" fields "${entry}")
    list(GET fields 0 case_id)
    list(GET fields 1 case_kind)
    list(GET fields 2 case_flags)

    # The legacy case is not re-run here: the assertions that constitute its
    # evidence already ran in VerifyReferenceHost.cmake and would have aborted
    # this script. Re-assert the two frozen anchors so the case is still
    # counted against real artefacts rather than against a comment.
    if(case_kind STREQUAL "legacy")
        list(APPEND cuexis_command_completed_ids "${case_id}")
        set(legacy_ok TRUE)
        if(NOT EXISTS "${report_path}")
            set(legacy_ok FALSE)
            cuexis_command_record_failure("${case_id}" "the legacy run record is missing")
        else()
            file(READ "${report_path}" legacy_report)
            if(NOT legacy_report MATCHES "host\\.frame index=1 mode=seek chartTimeMs=625 discontinuityId=1 objects=2 digest=${cuexis_golden_digest_value} algorithm=3")
                set(legacy_ok FALSE)
                cuexis_command_record_failure("${case_id}"
                    "the legacy record no longer carries the frozen golden frame")
            endif()
            if(NOT legacy_report MATCHES "host\\.load source=host-project identity=${golden_identity}")
                set(legacy_ok FALSE)
                cuexis_command_record_failure("${case_id}"
                    "the legacy record no longer carries the frozen reference identity")
            endif()
        endif()
        if(legacy_ok)
            list(APPEND cuexis_command_passed_ids "${case_id}")
        endif()
        list(APPEND cuexis_command_evidence "  ${case_id}: legacy run record re-asserted")
        continue()
    endif()

    list(APPEND cuexis_command_completed_ids "${case_id}")

    # Per-case input and report paths (section 8.2: independent report, removed
    # before the run so a previous success cannot masquerade as this one).
    set(case_input "${cuexis_command_output_dir}/${case_id}.cmd")
    set(case_report "${cuexis_command_output_dir}/${case_id}-report.txt")
    set(case_content "${cuexis_command_content}")
    if(case_flags STREQUAL "missing-file")
        set(case_input "${cuexis_command_output_dir}/n07a-does-not-exist.cmd")
    elseif(case_flags STREQUAL "wrong-content-root")
        # A real file, but not a project directory: the load must fail inside
        # the SDK and keep its own reason.
        set(case_content "${package_path}")
    endif()
    if(case_flags STREQUAL "missing-file")
        # The whole point of this case is that the path does not resolve, so an
        # input that exists would make the case test something else.
        if(EXISTS "${case_input}")
            cuexis_command_record_failure("${case_id}"
                "this case needs an absent input, but '${case_input}' exists")
            list(APPEND cuexis_command_evidence "  ${case_id}: input unexpectedly present")
            continue()
        endif()
    elseif(NOT EXISTS "${case_input}")
        cuexis_command_record_failure("${case_id}" "the command input is missing: '${case_input}'")
        list(APPEND cuexis_command_evidence "  ${case_id}: input missing")
        continue()
    endif()
    if(EXISTS "${case_report}")
        file(REMOVE "${case_report}")
    endif()

    # argv is assembled as a list and handed to execute_process directly. No
    # shell is involved, so a path with a space needs no quoting games.
    set(case_arguments
        --command-file "${case_input}"
        --content "${case_content}"
        --report "${case_report}")
    if(case_flags STREQUAL "expect-identity")
        list(APPEND case_arguments --expect-identity "${golden_identity}")
    elseif(case_flags STREQUAL "advance")
        list(APPEND case_arguments --advance 2)
    elseif(case_flags STREQUAL "package")
        list(APPEND case_arguments --package "${package_path}")
    elseif(case_flags STREQUAL "expect-digest")
        list(APPEND case_arguments --expect-digest "0=1")
    elseif(case_flags STREQUAL "duplicate-command-file")
        list(APPEND case_arguments --command-file "${case_input}")
    endif()

    # The sanitized environment is bounded to exactly this one child. PATH is
    # restored immediately after the result is captured, before any assertion
    # can raise FATAL_ERROR, so a failing case still leaves the caller with its
    # real toolchain PATH (section 8.4).
    set(case_saved_path "$ENV{PATH}")
    set(case_saved_library_path "$ENV{LD_LIBRARY_PATH}")
    set(ENV{PATH} "${clean_path}")
    if(WIN32)
        set(ENV{LD_LIBRARY_PATH} "")
    else()
        set(ENV{LD_LIBRARY_PATH} "${host_build}")
    endif()
    execute_process(
        COMMAND "${host_executable}" ${case_arguments}
        WORKING_DIRECTORY "${cuexis_command_output_dir}"
        TIMEOUT 30
        RESULT_VARIABLE case_result
        OUTPUT_VARIABLE case_output
        ERROR_VARIABLE case_error)
    set(ENV{PATH} "${case_saved_path}")
    set(ENV{LD_LIBRARY_PATH} "${case_saved_library_path}")

    # A timeout and a signal death are not negative evidence.
    if(case_result MATCHES "timeout|No such file|not found|cannot execute")
        cuexis_command_record_failure("${case_id}"
            "the host did not run to a normal exit (result '${case_result}'); a crash or timeout is not negative evidence")
        list(APPEND cuexis_command_evidence "  ${case_id}: abnormal exit '${case_result}'")
        continue()
    endif()

    if(NOT EXISTS "${case_report}")
        cuexis_command_record_failure("${case_id}"
            "the host produced no run record (exit ${case_result})\n    stdout: ${case_output}\n    stderr: ${case_error}")
        list(APPEND cuexis_command_evidence "  ${case_id}: no record (exit ${case_result})")
        continue()
    endif()
    file(READ "${case_report}" case_record)

    # ------------------------------------------------------------------
    # Parse the expectation file and check the record line by line.
    # ------------------------------------------------------------------
    file(READ "${cuexis_command_fixture_dir}/${case_id}.expect" case_expect_text)
    string(REPLACE "\r\n" "\n" case_expect_text "${case_expect_text}")
    # A ";" is ordinary data inside an expectation line, but CMake list
    # expansion would split the line in two and read the tail as a directive.
    # Escaping on the way into the list and restoring on the way out keeps a
    # fixture comment free to contain punctuation.
    string(REPLACE ";" "\\;" case_expect_text "${case_expect_text}")
    string(REPLACE "\n" ";" case_expect_lines "${case_expect_text}")
    set(case_failures_before "${cuexis_command_failures}")

    string(REGEX MATCHALL "host\\.frame [^\n]*" case_frames "${case_record}")
    list(LENGTH case_frames case_frame_total)
    set(case_frame_cursor 0)
    set(case_last_frame "")

    string(REGEX MATCH "host\\.clock [^\n]*" case_clock_line "${case_record}")
    set(case_expects_success FALSE)
    set(case_expects_failure FALSE)

    foreach(raw_line IN LISTS case_expect_lines)
        string(REPLACE "\\;" ";" raw_line "${raw_line}")
        string(STRIP "${raw_line}" line)
        if(line STREQUAL "" OR line MATCHES "^#")
            continue()
        endif()
        separate_arguments(line_parts UNIX_COMMAND "${line}")
        list(GET line_parts 0 directive)
        list(LENGTH line_parts directive_arity)
        math(EXPR directive_last "${directive_arity} - 1")

        # The directive is checked before arity so that a typo is reported as a
        # typo rather than as a wrong argument count.
        if(NOT directive IN_LIST cuexis_command_known_directives)
            cuexis_command_record_failure("${case_id}"
                "unknown .expect directive '${directive}' in ${case_id}.expect")
            continue()
        endif()
        # Arity is guarded before dispatch: a malformed expectation must produce
        # a readable assertion failure, not an out-of-range list(GET) error that
        # aborts the gate without naming the fixture line. require/absent take
        # two or more tokens because their literal is a whole report fragment;
        # every other directive has a fixed shape, and pinning it exactly stops
        # a stray trailing token from being ignored.
        set(arity_ok FALSE)
        if(directive STREQUAL "digest-anchor")
            if(directive_arity EQUAL 1)
                set(arity_ok TRUE)
            endif()
        elseif(directive STREQUAL "require" OR directive STREQUAL "absent")
            if(directive_arity GREATER_EQUAL 2)
                set(arity_ok TRUE)
            endif()
        elseif(directive STREQUAL "count" OR directive STREQUAL "require-count")
            if(directive_arity EQUAL 3)
                set(arity_ok TRUE)
            endif()
        else()
            if(directive_arity EQUAL 2)
                set(arity_ok TRUE)
            endif()
        endif()
        if(NOT arity_ok)
            cuexis_command_record_failure("${case_id}"
                "'${directive}' has the wrong number of arguments: '${line}'")
            continue()
        endif()

        if(directive STREQUAL "exit")
            list(GET line_parts 1 wanted)
            if(wanted STREQUAL "ok")
                set(case_expects_success TRUE)
            elseif(wanted STREQUAL "fail")
                set(case_expects_failure TRUE)
            else()
                cuexis_command_record_failure("${case_id}" "unknown exit expectation '${wanted}'")
            endif()

        elseif(directive STREQUAL "code")
            list(GET line_parts 1 wanted_code)
            cuexis_command_escape_literal("code=${wanted_code}" literal)
            if(NOT case_record MATCHES "host\\.diagnostic [^\n]*${literal}( |$)")
                cuexis_command_record_failure("${case_id}"
                    "the record carries no host.diagnostic with code=${wanted_code}")
            endif()

        elseif(directive STREQUAL "diagnostic-step")
            list(GET line_parts 1 wanted_step)
            cuexis_command_escape_literal("step=${wanted_step}" literal)
            if(NOT case_record MATCHES "host\\.diagnostic ${literal}( |$)")
                cuexis_command_record_failure("${case_id}"
                    "the record carries no host.diagnostic with step=${wanted_step}")
            endif()

        elseif(directive STREQUAL "frame")
            list(GET line_parts 1 spec)
            string(REPLACE "/" ";" spec_parts "${spec}")
            list(LENGTH spec_parts spec_arity)
            if(NOT spec_arity EQUAL 3)
                cuexis_command_record_failure("${case_id}" "malformed frame expectation '${spec}'")
                continue()
            endif()
            list(GET spec_parts 0 want_time)
            list(GET spec_parts 1 want_dt)
            list(GET spec_parts 2 want_id)
            if(case_frame_cursor GREATER_EQUAL case_frame_total)
                cuexis_command_record_failure("${case_id}"
                    "expected frame ${case_frame_cursor} (${spec}) but the record has only ${case_frame_total}")
                continue()
            endif()
            list(GET case_frames ${case_frame_cursor} actual_frame)
            # Field-by-field, not a verb-name match: the frozen prefix, the two
            # appended fields and the digest spelling are all pinned here.
            #
            # This is written as a real if() rather than through a helper that
            # takes the condition as a string. A condition assembled as text and
            # expanded into if(NOT ${condition}) is re-tokenized on whitespace,
            # so the quotes inside it stop being quotes; the frame comparison
            # then silently passes. The checker self-test in out/ covers this.
            if(NOT actual_frame MATCHES "chartTimeMs=${want_time} discontinuityId=${want_id} objects=[0-9]+ digest=[0-9]+ algorithm=3 cmdIndex=[0-9]+ simulationDeltaTimeMs=${want_dt}$")
                cuexis_command_record_failure("${case_id}"
                    "frame ${case_frame_cursor} does not match ${spec}: '${actual_frame}'")
            endif()
            set(case_last_frame "${actual_frame}")
            math(EXPR case_frame_cursor "${case_frame_cursor} + 1")

        elseif(directive STREQUAL "digest-anchor")
            if(case_last_frame STREQUAL "")
                cuexis_command_record_failure("${case_id}"
                    "digest-anchor appears before any frame expectation")
                continue()
            endif()
            cuexis_command_escape_literal("digest=${cuexis_golden_digest_value} " literal)
            if(NOT case_last_frame MATCHES "${literal}")
                cuexis_command_record_failure("${case_id}"
                    "the anchored frame does not carry the frozen reference digest: '${case_last_frame}'")
            endif()

        elseif(directive STREQUAL "count")
            list(GET line_parts 1 counter)
            list(GET line_parts 2 wanted_value)
            if(case_clock_line STREQUAL "")
                cuexis_command_record_failure("${case_id}"
                    "count ${counter} was expected but the record has no host.clock line")
                continue()
            endif()
            if(NOT case_clock_line MATCHES "${counter}=([0-9]+)")
                cuexis_command_record_failure("${case_id}"
                    "host.clock carries no ${counter} field: '${case_clock_line}'")
                continue()
            endif()
            if(NOT CMAKE_MATCH_1 STREQUAL "${wanted_value}")
                cuexis_command_record_failure("${case_id}"
                    "${counter} is ${CMAKE_MATCH_1}, expected ${wanted_value}")
            endif()

        elseif(directive STREQUAL "require")
            cuexis_command_join_range("${line_parts}" 1 ${directive_last} literal_text)
            cuexis_command_escape_literal("${literal_text}" literal)
            if(NOT case_record MATCHES "${literal}")
                cuexis_command_record_failure("${case_id}"
                    "the record is missing required text '${literal_text}'")
            endif()

        elseif(directive STREQUAL "require-count")
            # The count is the last token; everything between the directive and
            # it is the literal, so a literal may itself contain spaces.
            list(GET line_parts ${directive_last} wanted_count)
            math(EXPR literal_last "${directive_last} - 1")
            cuexis_command_join_range("${line_parts}" 1 ${literal_last} literal_text)
            cuexis_command_escape_literal("${literal_text}" literal)
            string(REGEX MATCHALL "${literal}" occurrences "${case_record}")
            list(LENGTH occurrences observed_count)
            if(NOT observed_count EQUAL wanted_count)
                cuexis_command_record_failure("${case_id}"
                    "'${literal_text}' appears ${observed_count} times, expected ${wanted_count}")
            endif()

        elseif(directive STREQUAL "absent")
            cuexis_command_join_range("${line_parts}" 1 ${directive_last} literal_text)
            cuexis_command_escape_literal("${literal_text}" literal)
            if(case_record MATCHES "${literal}")
                cuexis_command_record_failure("${case_id}"
                    "the record must not contain '${literal_text}'")
            endif()

        endif()
    endforeach()

    # Quantity, not just membership: a record with extra frames is as wrong as
    # one with too few.
    if(NOT case_frame_cursor EQUAL case_frame_total)
        cuexis_command_record_failure("${case_id}"
            "the record has ${case_frame_total} frames but only ${case_frame_cursor} were expected")
    endif()

    # Section 6.2 asks for two relations a per-frame triple comparison cannot
    # express, because the frame directive deliberately does not carry a digest
    # value: a digest that never changes when the sampled frame does is a
    # constant rather than a measurement, and the same sampled frame must always
    # produce the same digest. Without these, a host that printed one fixed
    # digest for every frame would pass every case that samples more than once.
    set(case_triples "")
    set(case_digests "")
    foreach(frame_line IN LISTS case_frames)
        string(REGEX MATCH
            "chartTimeMs=([-0-9]+) discontinuityId=([0-9]+) objects=([0-9]+) digest=([0-9]+) algorithm=([0-9]+) cmdIndex=([0-9]+) simulationDeltaTimeMs=([-0-9]+)"
            frame_shape "${frame_line}")
        if(NOT frame_shape)
            # A frame that is not in the command-mode shape is reported by the
            # frame directive itself; the relations below need the fields.
            continue()
        endif()
        set(frame_triple "${CMAKE_MATCH_1}/${CMAKE_MATCH_7}/${CMAKE_MATCH_2}")
        set(frame_digest "${CMAKE_MATCH_4}")
        list(APPEND case_triples "${frame_triple}")
        list(APPEND case_digests "${frame_digest}")
        # Three parallel lists rather than one joined key: splitting a joined
        # key back apart is one more thing that can silently desynchronize.
        list(LENGTH cuexis_command_seen_triples cuexis_seen_total)
        set(cuexis_seen_index 0)
        while(cuexis_seen_index LESS cuexis_seen_total)
            list(GET cuexis_command_seen_triples ${cuexis_seen_index} seen_triple)
            list(GET cuexis_command_seen_digests ${cuexis_seen_index} seen_digest)
            list(GET cuexis_command_seen_cases ${cuexis_seen_index} seen_case)
            if(seen_triple STREQUAL frame_triple AND NOT seen_digest STREQUAL frame_digest)
                cuexis_command_record_failure("${case_id}"
                    "frame (${frame_triple}) produced two digests: ${frame_digest} in ${case_id} and ${seen_digest} in ${seen_case}")
                break()
            endif()
            math(EXPR cuexis_seen_index "${cuexis_seen_index} + 1")
        endwhile()
        list(APPEND cuexis_command_seen_triples "${frame_triple}")
        list(APPEND cuexis_command_seen_digests "${frame_digest}")
        list(APPEND cuexis_command_seen_cases "${case_id}")
    endforeach()

    list(LENGTH case_frames case_frame_count)
    if(case_frame_count GREATER 1)
        list(REMOVE_DUPLICATES case_triples)
        list(LENGTH case_triples case_distinct_triples)
        if(case_distinct_triples GREATER 1)
            list(REMOVE_DUPLICATES case_digests)
            list(LENGTH case_digests case_distinct_digests)
            if(case_distinct_digests EQUAL 1)
                cuexis_command_record_failure("${case_id}"
                    "the record samples ${case_distinct_triples} different frames but reports the same digest for all of them")
            endif()
        endif()
    endif()

    # Exit code and summary are checked only after the record has been parsed,
    # so a mismatch reports the trace rather than just a number.
    if(case_expects_success)
        if(NOT case_result EQUAL 0)
            cuexis_command_record_failure("${case_id}"
                "expected exit 0 but the host exited ${case_result}\n    stderr: ${case_error}")
        endif()
        if(NOT case_record MATCHES "host\\.summary outcome=ok")
            cuexis_command_record_failure("${case_id}"
                "expected host.summary outcome=ok")
        endif()
    endif()
    if(case_expects_failure)
        if(case_result EQUAL 0)
            cuexis_command_record_failure("${case_id}"
                "expected a non-zero exit but the host exited 0")
        endif()
        if(NOT case_record MATCHES "host\\.summary outcome=failed")
            cuexis_command_record_failure("${case_id}"
                "expected host.summary outcome=failed")
        endif()
    endif()
    if(NOT case_expects_success AND NOT case_expects_failure)
        cuexis_command_record_failure("${case_id}" "the expectation file declares no exit expectation")
    endif()

    if(cuexis_command_failures STREQUAL "${case_failures_before}")
        list(APPEND cuexis_command_passed_ids "${case_id}")
        list(APPEND cuexis_command_evidence "  ${case_id}: ok (exit ${case_result}, ${case_frame_total} frames)")
    else()
        list(APPEND cuexis_command_evidence "  ${case_id}: FAILED (exit ${case_result})")
    endif()
    math(EXPR cuexis_command_case_total "${cuexis_command_case_total} + 1")
endforeach()

# ---------------------------------------------------------------------------
# Accounting. expected, completed and passed must be the same set. A gate that
# silently ran nothing is not a passing gate.
# ---------------------------------------------------------------------------
list(SORT cuexis_command_expected_ids)
list(SORT cuexis_command_completed_ids)
list(SORT cuexis_command_passed_ids)
list(LENGTH cuexis_command_expected_ids expected_count)
list(LENGTH cuexis_command_completed_ids completed_count)
list(LENGTH cuexis_command_passed_ids passed_count)

message(STATUS "Reference host command cases: expected ${expected_count}, completed ${completed_count}, passed ${passed_count}")

if(NOT cuexis_command_completed_ids STREQUAL cuexis_command_expected_ids)
    message(FATAL_ERROR
        "Command case accounting: completed set differs from the declared set\n"
        "  declared:  ${cuexis_command_expected_ids}\n"
        "  completed: ${cuexis_command_completed_ids}")
endif()
if(NOT cuexis_command_passed_ids STREQUAL cuexis_command_expected_ids)
    set(reportable "${cuexis_command_failures}")
    list(LENGTH reportable failure_count)
    set(shown "")
    set(shown_count 0)
    foreach(failure IN LISTS reportable)
        if(shown_count LESS 12)
            list(APPEND shown "  - ${failure}")
            math(EXPR shown_count "${shown_count} + 1")
        endif()
    endforeach()
    # One per line: the accounting is long enough that a semicolon-joined list
    # is unreadable exactly when it matters most.
    string(REPLACE ";" "\n" evidence_text "${cuexis_command_evidence}")
    string(REPLACE ";" "\n" shown_text "${shown}")
    message(FATAL_ERROR
        "Reference host command cases: ${failure_count} assertion failure(s) across "
        "${expected_count} declared cases.\n"
        "Evidence:\n${evidence_text}\n"
        "Failures (first ${shown_count} of ${failure_count}):\n${shown_text}")
endif()

message(STATUS "Reference host command-mode verification passed")
