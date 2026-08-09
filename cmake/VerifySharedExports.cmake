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
foreach(forbidden_symbol IN ITEMS
        RuntimeSession
        AssetDatabase
        entt::
        cuexis::world
        computeFrameDigestVersion
        normalizePresentationFrame
        detail@playback@cuexis
        cuexis::playback::detail::)
    if(symbol_output MATCHES "${forbidden_symbol}")
        message(FATAL_ERROR "Playback shared library exported internal symbol ${forbidden_symbol}")
    endif()
endforeach()

set(public_class_pattern "ChartClock|PlaybackSession|PlaybackSource|PreparedPlayback|RuntimeTimeline")
set(export_count 0)
string(REPLACE "\r\n" "\n" normalized_symbol_output "${symbol_output}")
string(REPLACE "\n" ";" symbol_lines "${normalized_symbol_output}")
foreach(symbol_line IN LISTS symbol_lines)
    if(CUEXIS_SYMBOL_TOOL_KIND STREQUAL "dumpbin")
        if(NOT symbol_line MATCHES
                "^[ \t]+[0-9]+[ \t]+[0-9A-Fa-f]+[ \t]+[0-9A-Fa-f]+[ \t]+([^ \t=]+)")
            continue()
        endif()
        set(exported_symbol "${CMAKE_MATCH_1}")
        math(EXPR export_count "${export_count} + 1")
        if(exported_symbol MATCHES "^\\?\\?[014](${public_class_pattern})@" OR
           exported_symbol MATCHES
                "^\\?[^@]+@(${public_class_pattern})@playback@cuexis@@" OR
           exported_symbol MATCHES "^\\?computeFrameDigest@playback@cuexis@@")
            continue()
        endif()
    else()
        if(NOT symbol_line MATCHES "[ \t][A-Za-z][ \t]+(.+)$")
            continue()
        endif()
        set(exported_symbol "${CMAKE_MATCH_1}")
        if(NOT exported_symbol MATCHES
                "^(cuexis::|(typeinfo|typeinfo name|vtable|VTT|guard variable|non-virtual thunk|virtual thunk) for cuexis::)")
            continue()
        endif()
        math(EXPR export_count "${export_count} + 1")
        if(exported_symbol MATCHES
                "^cuexis::playback::(${public_class_pattern})::" OR
           exported_symbol MATCHES "^cuexis::playback::computeFrameDigest\\(")
            continue()
        endif()
    endif()
    message(FATAL_ERROR "Playback shared library exported symbol outside the public allowlist: ${exported_symbol}")
endforeach()
if(export_count EQUAL 0)
    message(FATAL_ERROR "Playback shared export inspection did not parse any symbols")
endif()
