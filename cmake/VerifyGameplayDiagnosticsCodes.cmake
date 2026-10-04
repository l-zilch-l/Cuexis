#  Machine gate for the Gameplay V2 diagnostic code table (CM-D04 / P1-09, round 6).
#
#  Registered as the CTest case `cuexis_gameplay_diagnostics_codes`. It runs in CMake
#  script mode and compiles nothing:
#
#      cmake -DCUEXIS_SOURCE_DIR=<repository root> -P cmake/VerifyGameplayDiagnosticsCodes.cmake
#
#  `CUEXIS_DIAGNOSTIC_CODES_FILE` overrides the table path. The registered test passes the
#  canonical path explicitly; the override exists so that negative samples can be verified
#  against a temporary copy without editing the shipped table.
#
#  The gate fails with a non-zero exit and one explicit message when
#
#   1. the table file is missing, empty or not valid JSON, or a required top-level field
#      (schemaVersion, kind, categories, severityValues, faultedBehaviors, codes,
#      rejectionEntries, pendingRegistration) is absent, or kind / schemaVersion is wrong;
#   2. the same `code` string is registered twice;
#   3. a `codes` entry omits `category`, `severity` or `faulted`, or carries a value that is
#      not in `categories`, `severityValues` or `faultedBehaviors`;
#   4. an `input.*` or `geometry.*` code other than the three codes frozen by
#      GAMEPLAY_V2_ABI.md and GAMEPLAY_V2_SPEC.md 7.2 (input.continuous_unsupported,
#      input.direction_unsupported, geometry.inference_rejected) is registered, or one of
#      those three is missing;
#   5. a `rejectionEntries` row names a `stableRejectCode` that the `codes` array does not
#      publish, or leaves it null without the `pending_landing_batch` marker;
#   6. a `codes` entry has no non-empty `publicConsumptionSurface`, or a `pendingRegistration`
#      entry declares one. A public carrier belongs to a public registered code only, and the
#      src-only tokens are pending entries with status=not_registered_src_only: they must not
#      claim a public carrier. Without this rule a 29-of-29 field coverage could be read as
#      "every string in this table is public".
#
#  Four further integrity rules use the same freezes: `categories` must be exactly the nine
#  names of GAMEPLAY_V2_SPEC.md 9.2, `rejectionEntries` must be exactly R-01..R-19 each once,
#  a `pendingRegistration` entry may not re-register a code that `codes` already holds, and
#  the public / src-only split reported at the end must add up to the two array lengths.
#
#  The file is data, not policy: the gate checks that the table is internally complete and
#  that the frozen families did not grow. It does not decide whether a category assignment is
#  the right one; the table marks those derivations and this gate does not second-guess them.

if(NOT DEFINED CUEXIS_SOURCE_DIR OR CUEXIS_SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR
        "cuexis_gameplay_diagnostics_codes: CUEXIS_SOURCE_DIR must name the repository root")
endif()

function(cuexis_codes_fail)
    #  message() concatenates its arguments with no separator. A quoted argument that holds a
    #  CMake list is re-split into several arguments, so a `;` inside a failure message would
    #  be eaten by that split; no reason string below may therefore contain a semicolon. The
    #  reason is rebuilt from ARGV, and every split literal carries its own trailing space.
    string(JOIN "" cuexis_codes_reason ${ARGV})
    message(FATAL_ERROR "cuexis_gameplay_diagnostics_codes: ${cuexis_codes_reason}")
endfunction()

set(cuexis_codes_file
    "${CUEXIS_SOURCE_DIR}/schemas/cuexis.gameplay-diagnostics.v2.codes.json")
if(DEFINED CUEXIS_DIAGNOSTIC_CODES_FILE AND NOT CUEXIS_DIAGNOSTIC_CODES_FILE STREQUAL "")
    set(cuexis_codes_file "${CUEXIS_DIAGNOSTIC_CODES_FILE}")
endif()

if(NOT EXISTS "${cuexis_codes_file}")
    cuexis_codes_fail("code table not found: ${cuexis_codes_file}")
endif()

file(READ "${cuexis_codes_file}" cuexis_codes_document)
if(cuexis_codes_document STREQUAL "")
    cuexis_codes_fail("code table is empty: ${cuexis_codes_file}")
endif()

#  ---------------------------------------------------------------------------------------
#  Rule 1: valid JSON object with the required top-level fields, kind and schemaVersion.
#  ---------------------------------------------------------------------------------------

string(JSON cuexis_json_type ERROR_VARIABLE cuexis_json_error
    TYPE "${cuexis_codes_document}")
if(NOT cuexis_json_error STREQUAL "NOTFOUND")
    cuexis_codes_fail(
        "code table is not valid JSON (${cuexis_codes_file}): ${cuexis_json_error}")
endif()
if(NOT cuexis_json_type STREQUAL "OBJECT")
    cuexis_codes_fail("code table must be a JSON object, found ${cuexis_json_type}")
endif()

foreach(field IN ITEMS
        schemaVersion
        kind
        categories
        severityValues
        faultedBehaviors
        codes
        rejectionEntries
        pendingRegistration)
    string(JSON cuexis_field_type ERROR_VARIABLE cuexis_field_error
        TYPE "${cuexis_codes_document}" "${field}")
    if(NOT cuexis_field_error STREQUAL "NOTFOUND")
        cuexis_codes_fail("missing required top-level field '${field}'")
    endif()
endforeach()

string(JSON cuexis_kind ERROR_VARIABLE cuexis_error GET "${cuexis_codes_document}" kind)
if(NOT cuexis_kind STREQUAL "cuexis.gameplay-diagnostics.codes")
    cuexis_codes_fail(
        "top-level kind must be 'cuexis.gameplay-diagnostics.codes', found '${cuexis_kind}'")
endif()

string(JSON cuexis_schema_version ERROR_VARIABLE cuexis_error
    GET "${cuexis_codes_document}" schemaVersion)
if(NOT cuexis_schema_version STREQUAL "2")
    cuexis_codes_fail("top-level schemaVersion must be 2, found '${cuexis_schema_version}'")
endif()

#  ---------------------------------------------------------------------------------------
#  The nine category names of GAMEPLAY_V2_SPEC.md 9.2, used both as the membership set and
#  as the frozen count. Extending one requires a ruling and an edit to both this list and
#  the Spec.
#  ---------------------------------------------------------------------------------------

set(cuexis_spec_categories
    unknown_capability
    capability_disabled
    budget_exceeded
    ambiguous_migration
    non_terminating_source
    non_unique_solution
    invalid_relation
    late_policy_incomplete
    identity_closure_incomplete)

string(JSON cuexis_category_count ERROR_VARIABLE cuexis_error
    LENGTH "${cuexis_codes_document}" categories)
math(EXPR cuexis_category_last "${cuexis_category_count} - 1")

set(cuexis_registered_categories "")
if(cuexis_category_count GREATER 0)
    foreach(index RANGE 0 ${cuexis_category_last})
        string(JSON cuexis_category_name ERROR_VARIABLE cuexis_category_error
            GET "${cuexis_codes_document}" categories ${index} name)
        if(NOT cuexis_category_error STREQUAL "NOTFOUND")
            cuexis_codes_fail("categories[${index}] has no string 'name' member")
        endif()
        if(cuexis_category_name STREQUAL "")
            cuexis_codes_fail("categories[${index}] has an empty 'name'")
        endif()
        list(FIND cuexis_registered_categories "${cuexis_category_name}" cuexis_category_seen)
        if(NOT cuexis_category_seen EQUAL -1)
            cuexis_codes_fail("category '${cuexis_category_name}' is declared twice")
        endif()
        list(APPEND cuexis_registered_categories "${cuexis_category_name}")
    endforeach()
endif()

foreach(category IN LISTS cuexis_spec_categories)
    list(FIND cuexis_registered_categories "${category}" cuexis_category_index)
    if(cuexis_category_index EQUAL -1)
        cuexis_codes_fail(
            "GAMEPLAY_V2_SPEC.md 9.2 category '${category}' is missing from 'categories'")
    endif()
endforeach()

list(LENGTH cuexis_registered_categories cuexis_registered_category_count)
list(LENGTH cuexis_spec_categories cuexis_spec_category_count)
string(REPLACE ";" ", " cuexis_registered_categories_text "${cuexis_registered_categories}")
if(NOT cuexis_registered_category_count EQUAL cuexis_spec_category_count)
    cuexis_codes_fail(
        "'categories' must hold exactly the nine GAMEPLAY_V2_SPEC.md 9.2 names, found "
        "${cuexis_registered_category_count}: ${cuexis_registered_categories_text}")
endif()

#  ---------------------------------------------------------------------------------------
#  severity and faulted value sets.
#  ---------------------------------------------------------------------------------------

string(JSON cuexis_severity_count ERROR_VARIABLE cuexis_error
    LENGTH "${cuexis_codes_document}" severityValues)
math(EXPR cuexis_severity_last "${cuexis_severity_count} - 1")
set(cuexis_registered_severities "")
if(cuexis_severity_count GREATER 0)
    foreach(index RANGE 0 ${cuexis_severity_last})
        string(JSON cuexis_severity ERROR_VARIABLE cuexis_severity_error
            GET "${cuexis_codes_document}" severityValues ${index})
        if(NOT cuexis_severity_error STREQUAL "NOTFOUND")
            cuexis_codes_fail("severityValues[${index}] is not a JSON string")
        endif()
        if(cuexis_severity STREQUAL "")
            cuexis_codes_fail("severityValues[${index}] is empty")
        endif()
        list(APPEND cuexis_registered_severities "${cuexis_severity}")
    endforeach()
endif()
if(cuexis_registered_severities STREQUAL "")
    cuexis_codes_fail("severityValues must declare at least one severity")
endif()
string(REPLACE ";" ", " cuexis_registered_severities_text "${cuexis_registered_severities}")

string(JSON cuexis_faulted_count ERROR_VARIABLE cuexis_error
    LENGTH "${cuexis_codes_document}" faultedBehaviors)
math(EXPR cuexis_faulted_last "${cuexis_faulted_count} - 1")
set(cuexis_registered_faulted "")
if(cuexis_faulted_count GREATER 0)
    foreach(index RANGE 0 ${cuexis_faulted_last})
        string(JSON cuexis_faulted ERROR_VARIABLE cuexis_faulted_error
            GET "${cuexis_codes_document}" faultedBehaviors ${index})
        if(NOT cuexis_faulted_error STREQUAL "NOTFOUND")
            cuexis_codes_fail("faultedBehaviors[${index}] is not a JSON string")
        endif()
        if(cuexis_faulted STREQUAL "")
            cuexis_codes_fail("faultedBehaviors[${index}] is empty")
        endif()
        list(APPEND cuexis_registered_faulted "${cuexis_faulted}")
    endforeach()
endif()
if(cuexis_registered_faulted STREQUAL "")
    cuexis_codes_fail("faultedBehaviors must declare at least one behavior")
endif()
string(REPLACE ";" ", " cuexis_registered_faulted_text "${cuexis_registered_faulted}")

#  ---------------------------------------------------------------------------------------
#  Rules 2, 3 and 4 over the `codes` array.
#  ---------------------------------------------------------------------------------------

set(cuexis_frozen_family_codes
    input.continuous_unsupported
    input.direction_unsupported
    geometry.inference_rejected)
string(REPLACE ";" ", " cuexis_frozen_family_codes_text "${cuexis_frozen_family_codes}")

string(JSON cuexis_code_count ERROR_VARIABLE cuexis_error
    LENGTH "${cuexis_codes_document}" codes)
math(EXPR cuexis_code_last "${cuexis_code_count} - 1")
if(cuexis_code_count LESS 1)
    cuexis_codes_fail("'codes' must hold at least one entry")
endif()

set(cuexis_registered_codes "")
set(cuexis_published_reject_codes "")
if(cuexis_code_count GREATER 0)
foreach(index RANGE 0 ${cuexis_code_last})
    string(JSON cuexis_code ERROR_VARIABLE cuexis_code_error
        GET "${cuexis_codes_document}" codes ${index} code)
    if(NOT cuexis_code_error STREQUAL "NOTFOUND")
        cuexis_codes_fail("codes[${index}] has no string 'code' member")
    endif()
    if(cuexis_code STREQUAL "")
        cuexis_codes_fail("codes[${index}] has an empty 'code'")
    endif()

    #  Rule 2: the same code string may be registered only once.
    list(FIND cuexis_registered_codes "${cuexis_code}" cuexis_code_seen)
    if(NOT cuexis_code_seen EQUAL -1)
        cuexis_codes_fail("duplicate code '${cuexis_code}' at codes[${index}]")
    endif()
    list(APPEND cuexis_registered_codes "${cuexis_code}")

    #  Rule 3: every entry carries a complete category / severity / faulted mapping.
    string(JSON cuexis_code_category ERROR_VARIABLE cuexis_code_category_error
        GET "${cuexis_codes_document}" codes ${index} category)
    if(NOT cuexis_code_category_error STREQUAL "NOTFOUND")
        cuexis_codes_fail("codes[${index}] ('${cuexis_code}') has no 'category'")
    endif()
    list(FIND cuexis_registered_categories "${cuexis_code_category}" cuexis_category_index)
    if(cuexis_category_index EQUAL -1)
        cuexis_codes_fail(
            "codes[${index}] ('${cuexis_code}') has category '${cuexis_code_category}', "
            "which is not one of the registered categories: "
            "${cuexis_registered_categories_text}")
    endif()

    string(JSON cuexis_code_severity ERROR_VARIABLE cuexis_code_severity_error
        GET "${cuexis_codes_document}" codes ${index} severity)
    if(NOT cuexis_code_severity_error STREQUAL "NOTFOUND")
        cuexis_codes_fail("codes[${index}] ('${cuexis_code}') has no 'severity'")
    endif()
    list(FIND cuexis_registered_severities "${cuexis_code_severity}" cuexis_severity_index)
    if(cuexis_severity_index EQUAL -1)
        cuexis_codes_fail(
            "codes[${index}] ('${cuexis_code}') has severity '${cuexis_code_severity}', "
            "which is not one of the registered severities: "
            "${cuexis_registered_severities_text}")
    endif()

    string(JSON cuexis_code_faulted ERROR_VARIABLE cuexis_code_faulted_error
        GET "${cuexis_codes_document}" codes ${index} faulted)
    if(NOT cuexis_code_faulted_error STREQUAL "NOTFOUND")
        cuexis_codes_fail("codes[${index}] ('${cuexis_code}') has no 'faulted'")
    endif()
    list(FIND cuexis_registered_faulted "${cuexis_code_faulted}" cuexis_faulted_index)
    if(cuexis_faulted_index EQUAL -1)
        cuexis_codes_fail(
            "codes[${index}] ('${cuexis_code}') has faulted '${cuexis_code_faulted}', "
            "which is not one of the registered behaviors: "
            "${cuexis_registered_faulted_text}")
    endif()

    #  Rule 4: the input and geometry families are frozen to the three ABI codes.
    string(REGEX MATCH "^(input|geometry)\\." cuexis_family_prefix "${cuexis_code}")
    if(NOT cuexis_family_prefix STREQUAL "")
        list(FIND cuexis_frozen_family_codes "${cuexis_code}" cuexis_frozen_index)
        if(cuexis_frozen_index EQUAL -1)
            cuexis_codes_fail(
                "code '${cuexis_code}' extends the frozen ${cuexis_family_prefix}* family. "
                "Only ${cuexis_frozen_family_codes_text} may be registered there")
        endif()
    endif()

    #  `stableRejectCode` must be present. A JSON null marks a code that is not a
    #  capability registry reject code; a string publishes the reject code for rule 5.
    string(JSON cuexis_reject_type ERROR_VARIABLE cuexis_reject_error
        TYPE "${cuexis_codes_document}" codes ${index} stableRejectCode)
    if(NOT cuexis_reject_error STREQUAL "NOTFOUND")
        cuexis_codes_fail("codes[${index}] ('${cuexis_code}') has no 'stableRejectCode'")
    endif()
    if(NOT cuexis_reject_type STREQUAL "NULL")
        string(JSON cuexis_reject_code ERROR_VARIABLE cuexis_reject_value_error
            GET "${cuexis_codes_document}" codes ${index} stableRejectCode)
        if(NOT cuexis_reject_value_error STREQUAL "NOTFOUND")
            cuexis_codes_fail(
                "codes[${index}] ('${cuexis_code}') has a non-string 'stableRejectCode'")
        endif()
        if(cuexis_reject_code STREQUAL "")
            cuexis_codes_fail(
                "codes[${index}] ('${cuexis_code}') has an empty 'stableRejectCode'. "
                "Use JSON null when the code is not a registry reject code")
        endif()
        list(APPEND cuexis_published_reject_codes "${cuexis_reject_code}")
    endif()
endforeach()
endif()

foreach(frozen IN LISTS cuexis_frozen_family_codes)
    list(FIND cuexis_registered_codes "${frozen}" cuexis_frozen_present)
    if(cuexis_frozen_present EQUAL -1)
        cuexis_codes_fail(
            "frozen code '${frozen}' is missing from 'codes'. The three ABI input/geometry "
            "codes are not optional")
    endif()
endforeach()

#  ---------------------------------------------------------------------------------------
#  Rule 5 plus the R-01..R-19 freeze over `rejectionEntries`.
#  ---------------------------------------------------------------------------------------

string(JSON cuexis_rejection_count ERROR_VARIABLE cuexis_error
    LENGTH "${cuexis_codes_document}" rejectionEntries)
math(EXPR cuexis_rejection_last "${cuexis_rejection_count} - 1")
if(NOT cuexis_rejection_count EQUAL 19)
    cuexis_codes_fail(
        "GAMEPLAY_V2_SPEC.md 7.2 freezes exactly 19 rejection entries (R-01..R-19), "
        "'rejectionEntries' holds ${cuexis_rejection_count}")
endif()

set(cuexis_rejection_ids "")
foreach(index RANGE 0 ${cuexis_rejection_last})
    string(JSON cuexis_rejection_id ERROR_VARIABLE cuexis_rejection_id_error
        GET "${cuexis_codes_document}" rejectionEntries ${index} id)
    if(NOT cuexis_rejection_id_error STREQUAL "NOTFOUND")
        cuexis_codes_fail("rejectionEntries[${index}] has no string 'id'")
    endif()
    if(NOT cuexis_rejection_id MATCHES "^R-[0-9][0-9]$")
        cuexis_codes_fail(
            "rejectionEntries[${index}] has id '${cuexis_rejection_id}', "
            "which is not of the form R-NN")
    endif()
    list(FIND cuexis_rejection_ids "${cuexis_rejection_id}" cuexis_rejection_seen)
    if(NOT cuexis_rejection_seen EQUAL -1)
        cuexis_codes_fail("rejection entry '${cuexis_rejection_id}' is declared twice")
    endif()
    list(APPEND cuexis_rejection_ids "${cuexis_rejection_id}")

    string(JSON cuexis_rejection_reject_type ERROR_VARIABLE cuexis_rejection_type_error
        TYPE "${cuexis_codes_document}" rejectionEntries ${index} stableRejectCode)
    if(NOT cuexis_rejection_type_error STREQUAL "NOTFOUND")
        cuexis_codes_fail(
            "rejection entry ${cuexis_rejection_id} has no 'stableRejectCode' member")
    endif()

    if(cuexis_rejection_reject_type STREQUAL "NULL")
        #  A rejection entry without a frozen code must say so explicitly.
        string(JSON cuexis_rejection_status ERROR_VARIABLE cuexis_rejection_status_error
            GET "${cuexis_codes_document}" rejectionEntries ${index} status)
        if(NOT cuexis_rejection_status_error STREQUAL "NOTFOUND")
            cuexis_codes_fail(
                "rejection entry ${cuexis_rejection_id} has a null 'stableRejectCode' but "
                "no 'status'. A pending entry must carry status=pending_landing_batch")
        endif()
        if(NOT cuexis_rejection_status STREQUAL "pending_landing_batch")
            cuexis_codes_fail(
                "rejection entry ${cuexis_rejection_id} has a null 'stableRejectCode' with "
                "status '${cuexis_rejection_status}'. Expected pending_landing_batch")
        endif()
    else()
        string(JSON cuexis_rejection_code ERROR_VARIABLE cuexis_rejection_code_error
            GET "${cuexis_codes_document}" rejectionEntries ${index} stableRejectCode)
        if(NOT cuexis_rejection_code_error STREQUAL "NOTFOUND")
            cuexis_codes_fail(
                "rejection entry ${cuexis_rejection_id} has a non-string 'stableRejectCode'")
        endif()
        if(cuexis_rejection_code STREQUAL "")
            cuexis_codes_fail(
                "rejection entry ${cuexis_rejection_id} has an empty 'stableRejectCode'. "
                "Use JSON null plus status=pending_landing_batch when no code is frozen yet")
        endif()
        list(FIND cuexis_published_reject_codes "${cuexis_rejection_code}"
            cuexis_rejection_code_index)
        if(cuexis_rejection_code_index EQUAL -1)
            cuexis_codes_fail(
                "rejection entry ${cuexis_rejection_id} names stableRejectCode "
                "'${cuexis_rejection_code}', which no 'codes' entry publishes. Add the code "
                "to the table first")
        endif()
    endif()
endforeach()

#  ---------------------------------------------------------------------------------------
#  Pending registrations may describe work but may not re-register a live code.
#  ---------------------------------------------------------------------------------------

string(JSON cuexis_pending_count ERROR_VARIABLE cuexis_error
    LENGTH "${cuexis_codes_document}" pendingRegistration)
math(EXPR cuexis_pending_last "${cuexis_pending_count} - 1")
if(cuexis_pending_count GREATER 0)
foreach(index RANGE 0 ${cuexis_pending_last})
    string(JSON cuexis_pending_type ERROR_VARIABLE cuexis_pending_type_error
        TYPE "${cuexis_codes_document}" pendingRegistration ${index} code)
    if(NOT cuexis_pending_type_error STREQUAL "NOTFOUND")
        cuexis_codes_fail("pendingRegistration[${index}] has no 'code' member")
    endif()
    if(cuexis_pending_type STREQUAL "NULL")
        continue()
    endif()
    string(JSON cuexis_pending_code ERROR_VARIABLE cuexis_pending_code_error
        GET "${cuexis_codes_document}" pendingRegistration ${index} code)
    if(NOT cuexis_pending_code_error STREQUAL "NOTFOUND")
        cuexis_codes_fail("pendingRegistration[${index}] has a non-string 'code'")
    endif()
    list(FIND cuexis_registered_codes "${cuexis_pending_code}" cuexis_pending_registered)
    if(NOT cuexis_pending_registered EQUAL -1)
        cuexis_codes_fail(
            "code '${cuexis_pending_code}' is listed as pendingRegistration[${index}] but "
            "is already registered. Remove it from one of the two lists")
    endif()
endforeach()
endif()

#  ---------------------------------------------------------------------------------------
#  Rule 6: the public carrier and the src-only tokens are disjoint.
#
#  A non-null `publicConsumptionSurface` marks a public registered code. It may appear only on
#  a `codes` entry, and there it must be a non-empty string. Every `pendingRegistration` entry
#  must carry an explicit JSON null: the thirteen src-only tokens and the open slots with no
#  code string chosen yet are both non-public, and a pending entry that names a public carrier
#  is a table error. The split is printed at the end so that a 29-of-29 field coverage can
#  never be read as "everything in this table is public".
#  ---------------------------------------------------------------------------------------

set(cuexis_public_surface_count 0)
foreach(index RANGE 0 ${cuexis_code_last})
    string(JSON cuexis_surface_type ERROR_VARIABLE cuexis_surface_error
        TYPE "${cuexis_codes_document}" codes ${index} publicConsumptionSurface)
    if(NOT cuexis_surface_error STREQUAL "NOTFOUND")
        cuexis_codes_fail(
            "codes[${index}] has no 'publicConsumptionSurface'. A public registered code must name "
            "the existing carrier it is consumed through")
    endif()
    if(NOT cuexis_surface_type STREQUAL "STRING")
        cuexis_codes_fail(
            "codes[${index}] 'publicConsumptionSurface' has JSON type ${cuexis_surface_type}. A public "
            "code must carry a non-empty carrier string. A string with no public carrier must not be "
            "listed in 'codes' at all")
    endif()
    string(JSON cuexis_surface ERROR_VARIABLE cuexis_surface_value_error
        GET "${cuexis_codes_document}" codes ${index} publicConsumptionSurface)
    if(cuexis_surface STREQUAL "")
        cuexis_codes_fail(
            "codes[${index}] has an empty 'publicConsumptionSurface'. A public carrier description "
            "may not be blank")
    endif()
    math(EXPR cuexis_public_surface_count "${cuexis_public_surface_count} + 1")
endforeach()

set(cuexis_src_only_count 0)
foreach(index RANGE 0 ${cuexis_pending_last})
    string(JSON cuexis_pending_surface_type ERROR_VARIABLE cuexis_pending_surface_error
        TYPE "${cuexis_codes_document}" pendingRegistration ${index} publicConsumptionSurface)
    if(NOT cuexis_pending_surface_error STREQUAL "NOTFOUND")
        cuexis_codes_fail(
            "pendingRegistration[${index}] has no 'publicConsumptionSurface'. A pending entry must "
            "state explicitly that it has no public carrier. Use JSON null")
    endif()
    if(NOT cuexis_pending_surface_type STREQUAL "NULL")
        string(JSON cuexis_pending_status ERROR_VARIABLE cuexis_pending_status_error
            GET "${cuexis_codes_document}" pendingRegistration ${index} status)
        if(cuexis_pending_status_error STREQUAL "NOTFOUND"
                AND cuexis_pending_status STREQUAL "not_registered_src_only")
            cuexis_codes_fail(
                "pendingRegistration[${index}] is a src-only token (status = not_registered_src_only) "
                "but declares a public carrier of JSON type ${cuexis_pending_surface_type}. A src-only "
                "token must not claim a public consumption surface. Use JSON null")
        endif()
        cuexis_codes_fail(
            "pendingRegistration[${index}] declares a public carrier of JSON type "
            "${cuexis_pending_surface_type}. Nothing in 'pendingRegistration' is public. Use JSON null")
    endif()
    string(JSON cuexis_pending_status ERROR_VARIABLE cuexis_pending_status_error
        GET "${cuexis_codes_document}" pendingRegistration ${index} status)
    if(cuexis_pending_status_error STREQUAL "NOTFOUND"
            AND cuexis_pending_status STREQUAL "not_registered_src_only")
        math(EXPR cuexis_src_only_count "${cuexis_src_only_count} + 1")
    endif()
endforeach()

list(LENGTH cuexis_registered_codes cuexis_registered_code_count)
set(cuexis_input_geometry_count 0)
foreach(code IN LISTS cuexis_registered_codes)
    if(code MATCHES "^(input|geometry)\\.")
        math(EXPR cuexis_input_geometry_count "${cuexis_input_geometry_count} + 1")
    endif()
endforeach()

message(STATUS
    "gameplay diagnostics code table OK: ${cuexis_registered_code_count} codes "
    "(${cuexis_input_geometry_count} frozen input/geometry), "
    "${cuexis_registered_category_count} categories, "
    "${cuexis_severity_count} severities, ${cuexis_faulted_count} faulted behaviors, "
    "${cuexis_rejection_count} rejection entries, ${cuexis_pending_count} pending entries")
message(STATUS
    "gameplay diagnostics public surface split: ${cuexis_public_surface_count} public codes carry a "
    "publicConsumptionSurface, ${cuexis_pending_count} pending entries carry none "
    "(${cuexis_src_only_count} of them are src-only tokens)")

