# Drives the RR-4 shared Player evidence for the execution-host access bridge. The real Player and
# the in-repo test executable must import only the experimental Playback shared library this build
# produced; the library must export every allowed internal entry; then the bridge and a production
# Pause/Resume/Seek/Close host case run against that binary.
foreach(required IN ITEMS CUEXIS_PLAYER CUEXIS_PLAYER_DIR CUEXIS_PLAYBACK_DLL
        CUEXIS_PLAYBACK_DLL_NAME CUEXIS_CONSUMER)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "rr_sdk_execution_host_shared is missing ${required}")
    endif()
endforeach()
if(NOT EXISTS "${CUEXIS_PLAYER}")
    message(FATAL_ERROR "Player executable not built: ${CUEXIS_PLAYER}")
endif()
if(NOT EXISTS "${CUEXIS_CONSUMER}")
    message(FATAL_ERROR "Shared execution-host consumer not built: ${CUEXIS_CONSUMER}")
endif()
if(NOT EXISTS "${CUEXIS_PLAYBACK_DLL}")
    message(FATAL_ERROR "Experimental Playback shared library not built: ${CUEXIS_PLAYBACK_DLL}")
endif()

# Derive dumpbin from the linker directory so the gate does not depend on a vcvars shell.
if(CUEXIS_MSVC)
    get_filename_component(linker_dir "${CUEXIS_LINKER}" DIRECTORY)
    set(dumpbin "${linker_dir}/dumpbin.exe")
    if(NOT EXISTS "${dumpbin}")
        message(FATAL_ERROR "dumpbin is unavailable next to the linker: ${dumpbin}")
    endif()
    execute_process(COMMAND "${dumpbin}" /nologo /dependents "${CUEXIS_PLAYER}"
                    RESULT_VARIABLE dependents_status OUTPUT_VARIABLE dependents
                    ERROR_VARIABLE dependents_error)
    if(NOT dependents_status EQUAL 0)
        message(FATAL_ERROR "Player import inspection failed: ${dependents_error}")
    endif()
    string(REPLACE "\r\n" "\n" dependents "${dependents}")
    string(REPLACE "\n" ";" dependents_lines "${dependents}")
    set(playback_imports "")
    foreach(line IN LISTS dependents_lines)
        if(line MATCHES "([A-Za-z0-9_.-]*cuexis_playback[A-Za-z0-9_.-]*[.]dll)")
            list(APPEND playback_imports "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    list(LENGTH playback_imports playback_import_count)
    if(NOT playback_import_count EQUAL 1)
        message(FATAL_ERROR
            "Player must import exactly one Playback shared library, got ${playback_import_count}: "
            "${playback_imports}")
    endif()
    list(GET playback_imports 0 player_import)
    if(NOT player_import STREQUAL "${CUEXIS_PLAYBACK_DLL_NAME}")
        message(FATAL_ERROR
            "Player imports '${player_import}' but this build produced "
            "'${CUEXIS_PLAYBACK_DLL_NAME}'")
    endif()

    execute_process(COMMAND "${dumpbin}" /nologo /exports "${CUEXIS_PLAYBACK_DLL}"
                    RESULT_VARIABLE exports_status OUTPUT_VARIABLE exports
                    ERROR_VARIABLE exports_error)
    if(NOT exports_status EQUAL 0)
        message(FATAL_ERROR "Playback export inspection failed: ${exports_error}")
    endif()
    foreach(method IN ITEMS
            enterExecutionFence commitExecutionCandidate releaseExecutionFence executionFence
            completeExecutionClose validateExecutionControlBinding validateExecutionControlProof
            validateExecutionFrameContext acceptExecutionControl executionOwnerState
            executionStepEffects executionAdvanceRetention retireExecutionStep pauseExecutionStep
            resumeExecutionStep seekExecutionStep)
        string(FIND "${exports}" "?${method}@ExecutionHostAccess@detail@playback@cuexis@@" position)
        if(position EQUAL -1)
            message(FATAL_ERROR
                "Experimental Playback library does not export ExecutionHostAccess::${method}")
        endif()
    endforeach()
else()
    message(STATUS
        "rr_sdk_execution_host_shared: import/symbol inspection is MSVC-only in this gate; "
        "bridge and host behavior still run")
endif()

execute_process(COMMAND "${CUEXIS_CONSUMER}" "rr_sdk_execution_host_shared_linkage"
                RESULT_VARIABLE bridge_status OUTPUT_VARIABLE bridge_output
                ERROR_VARIABLE bridge_error TIMEOUT 120)
message(STATUS "bridge output:\n${bridge_output}${bridge_error}")
if(NOT bridge_status EQUAL 0)
    message(FATAL_ERROR "execution-host bridge case failed with ${bridge_status}")
endif()

execute_process(COMMAND "${CUEXIS_CONSUMER}"
                    "B5 realtime main host cuts after inspection and drives Pause Resume Seek Close"
                RESULT_VARIABLE host_status OUTPUT_VARIABLE host_output
                ERROR_VARIABLE host_error TIMEOUT 240)
message(STATUS "host output:\n${host_output}${host_error}")
if(NOT host_status EQUAL 0)
    message(FATAL_ERROR "production host Pause/Resume/Seek/Close case failed with ${host_status}")
endif()

if(CUEXIS_MSVC)
    message(STATUS
        "rr_sdk_execution_host_shared: Player and consumer import ${CUEXIS_PLAYBACK_DLL_NAME}; "
        "internal exports and host behavior passed")
else()
    message(STATUS
        "rr_sdk_execution_host_shared: bridge and host behavior passed (non-MSVC inspection)")
endif()
