foreach(required_variable IN ITEMS CUEXIS_SHARED_LIBRARY CUEXIS_SYMBOL_TOOL CUEXIS_SYMBOL_TOOL_KIND)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

if(CUEXIS_SYMBOL_TOOL_KIND STREQUAL "dumpbin")
    execute_process(
        COMMAND "${CUEXIS_SYMBOL_TOOL}" /nologo /exports "${CUEXIS_SHARED_LIBRARY}"
        RESULT_VARIABLE symbol_result
        OUTPUT_VARIABLE symbol_output
        ERROR_VARIABLE symbol_error)
else()
    execute_process(
        COMMAND "${CUEXIS_SYMBOL_TOOL}" -D -C --defined-only "${CUEXIS_SHARED_LIBRARY}"
        RESULT_VARIABLE symbol_result
        OUTPUT_VARIABLE symbol_output
        ERROR_VARIABLE symbol_error)
endif()
if(NOT symbol_result EQUAL 0)
    message(FATAL_ERROR "Shared symbol inspection failed: ${symbol_error}")
endif()

foreach(required_symbol IN ITEMS
        PlaybackSession
        PlaybackSource
        RuntimeTimeline
        acquirePresentationResource
        computeFrameDigest
        presentationManifest
        validatePresentation)
    if(NOT symbol_output MATCHES "${required_symbol}")
        message(FATAL_ERROR "Playback shared library does not export ${required_symbol}")
    endif()
endforeach()
foreach(forbidden_symbol IN ITEMS RuntimeSession AssetDatabase entt:: cuexis::world)
    if(symbol_output MATCHES "${forbidden_symbol}")
        message(FATAL_ERROR "Playback shared library exported internal symbol ${forbidden_symbol}")
    endif()
endforeach()
