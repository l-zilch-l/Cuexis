# Characterization gates for frozen process contracts.
#
# These tests execute repository tooling rather than C++ test binaries, so they
# need a Python interpreter. The interpreter is discovered at configure time;
# when none is available the tests are not registered at all, which keeps the
# gate honest (it never reports a passing test that did not run).
find_package(Python3 COMPONENTS Interpreter QUIET)

if(NOT Python3_Interpreter_FOUND)
    message(STATUS "contract checks skipped: no Python3 interpreter found")
    return()
endif()

# S6-D07 (version gate) behavioural characterization: same-day build +1,
# cross-day build=1, trusted baseline and the bootstrap refusal.
add_test(
    NAME cuexis_contract_version_gate
    COMMAND "${Python3_EXECUTABLE}" -B
        "${PROJECT_SOURCE_DIR}/tools/check_version_gate_tests.py"
)

set_tests_properties(cuexis_contract_version_gate PROPERTIES
    LABELS "contract;stage6;s6-d07;version-gate"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    TIMEOUT 300
)

# S6-A2 characterization: schemas, entry fixtures, identity/media goldens,
# productization boundaries, S6-D07 wiring and the S6-D08 named host.
add_test(
    NAME cuexis_contract_s6_a2
    COMMAND "${Python3_EXECUTABLE}" -B
        "${PROJECT_SOURCE_DIR}/tools/check_stage6_a2.py"
)

set_tests_properties(cuexis_contract_s6_a2 PROPERTIES
    LABELS "contract;stage6;s6-a2;s6-d07;s6-d08"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    TIMEOUT 300
)
