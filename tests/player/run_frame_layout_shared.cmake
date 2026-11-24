# Drives the shared Player distribution together with the in-repo shared consumer of the
# experimental laid-out frame surface. Both must run against the Cuexis::Playback binary this
# build produced; a header-only or static substitute cannot satisfy this test.

foreach(required IN ITEMS CUEXIS_PLAYER CUEXIS_CONSUMER CUEXIS_PLAYER_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "rr_frame_layout_shared is missing ${required}")
    endif()
endforeach()

if(NOT EXISTS "${CUEXIS_PLAYER}")
    message(FATAL_ERROR "Player executable not built: ${CUEXIS_PLAYER}")
endif()
if(NOT EXISTS "${CUEXIS_CONSUMER}")
    message(FATAL_ERROR
        "Shared frame-layout consumer not built: ${CUEXIS_CONSUMER}; "
        "build the cuexis_frame_layout_shared_consumer target in this preset")
endif()

execute_process(
    COMMAND "${CUEXIS_CONSUMER}"
    RESULT_VARIABLE consumer_result
    OUTPUT_VARIABLE consumer_output
    ERROR_VARIABLE consumer_error
    TIMEOUT 120
)
message(STATUS "consumer output:\n${consumer_output}${consumer_error}")
if(NOT consumer_result EQUAL 0)
    message(FATAL_ERROR "shared frame-layout consumer failed with ${consumer_result}")
endif()

execute_process(
    COMMAND "${CUEXIS_PLAYER}" --smoke-test
    WORKING_DIRECTORY "${CUEXIS_PLAYER_DIR}"
    RESULT_VARIABLE player_result
    OUTPUT_VARIABLE player_output
    ERROR_VARIABLE player_error
    TIMEOUT 240
)
message(STATUS "player smoke output:\n${player_output}${player_error}")
if(NOT player_result EQUAL 0)
    if("${player_output}${player_error}" MATCHES
       "render\\.opengl\\.(version_unsupported|backend_unavailable)|platform\\.sdl\\.window_create_failed")
        # The shared Player path needs a usable OpenGL context; hosted runners without a GL
        # driver cannot provide one, so the smoke leg is recorded as not checked instead of
        # turning an environment limitation into a product failure.
        message(STATUS
            "rr_frame_layout_shared: consumer passed; real Player smoke NOT CHECKED "
            "(no usable OpenGL context on this host)")
        return()
    endif()
    message(FATAL_ERROR "real Player smoke run failed with ${player_result}")
endif()

message(STATUS "rr_frame_layout_shared: consumer and real Player smoke both passed")
