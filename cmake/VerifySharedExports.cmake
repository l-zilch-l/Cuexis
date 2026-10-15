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
elseif(CUEXIS_SYMBOL_TOOL_KIND STREQUAL "objdump")
    execute_process(
        COMMAND "${CUEXIS_SYMBOL_TOOL}" -p "${CUEXIS_SHARED_LIBRARY}"
        RESULT_VARIABLE symbol_result
        OUTPUT_VARIABLE symbol_output
        ERROR_VARIABLE symbol_error)
else()
    execute_process(
        COMMAND "${CUEXIS_SYMBOL_TOOL}" -D --defined-only "${CUEXIS_SHARED_LIBRARY}"
        RESULT_VARIABLE symbol_result
        OUTPUT_VARIABLE symbol_output
        ERROR_VARIABLE symbol_error)
endif()
if(NOT symbol_result EQUAL 0)
    message(FATAL_ERROR "Shared symbol inspection failed: ${symbol_error}")
endif()

if(CUEXIS_SYMBOL_TOOL_KIND STREQUAL "objdump")
    # Required symbols must be exported, not merely present in PE import/debug tables.
    set(required_symbol_output "")
    string(REPLACE "\r\n" "\n" pe_symbol_output "${symbol_output}")
    string(REPLACE "\n" ";" pe_symbol_lines "${pe_symbol_output}")
    # Binutils prints either "[ordinal] name" or "[ordinal] +base[base] hint name".
    set(pe_export_name_pattern
        "^[ \t]*\\[[ \t]*[0-9]+\\][ \t]+(\\+base\\[[ \t]*[0-9]+\\][ \t]+[0-9A-Fa-f]+[ \t]+)?([^ \t]+)[ \t]*$")
    foreach(pe_line IN LISTS pe_symbol_lines)
        if(pe_line MATCHES "${pe_export_name_pattern}")
            string(APPEND required_symbol_output "${CMAKE_MATCH_2}\n")
        endif()
    endforeach()
elseif(CUEXIS_SYMBOL_TOOL_KIND STREQUAL "dumpbin")
    set(required_symbol_output "${symbol_output}")
else()
    execute_process(
        COMMAND "${CUEXIS_SYMBOL_TOOL}" -D -C --defined-only "${CUEXIS_SHARED_LIBRARY}"
        RESULT_VARIABLE demangled_symbol_result
        OUTPUT_VARIABLE required_symbol_output
        ERROR_VARIABLE demangled_symbol_error)
    if(NOT demangled_symbol_result EQUAL 0)
        message(FATAL_ERROR "Shared symbol demangling failed: ${demangled_symbol_error}")
    endif()
endif()

foreach(required_symbol IN ITEMS
        PlaybackSession
        PlaybackSource
        RuntimeTimeline
        acquirePresentationResource
        computeFrameDigest
        fromCxcFile
        fromCxcMemory
        fromTypedProjectSource
        prepareLoad
        prepareReload
        presentationManifest
        reload
        semanticIdentity
        validatePresentation)
    if(NOT required_symbol_output MATCHES "${required_symbol}")
        message(FATAL_ERROR "Playback shared library does not export ${required_symbol}")
    endif()
endforeach()
set(public_class_pattern "ChartClock|RealtimeClock|PlaybackSession|PlaybackSource|PreparedPlayback|RuntimeTimeline")
set(public_mangled_classes "10ChartClock|13RealtimeClock|15PlaybackSession|14PlaybackSource|16PreparedPlayback|15RuntimeTimeline")
set(public_function_pattern "computeFrameDigest")
set(public_mangled_functions "18computeFrameDigest")
if(CUEXIS_GAMEPLAY_CANDIDATE)
    string(APPEND public_class_pattern "|GameplayContent|GameplayResult|GameplayCheckpoint|GameplayReplay|GameplaySnapshot")
    string(APPEND public_mangled_classes "|15GameplayContent|14GameplayResult|18GameplayCheckpoint|14GameplayReplay|16GameplaySnapshot")
    string(APPEND public_function_pattern "|gameplayCapabilities|encodeGameplayConfiguration|decodeGameplayConfiguration|computeGameplayReconciliationIdentity")
    string(APPEND public_mangled_functions "|20gameplayCapabilities|27encodeGameplayConfiguration|27decodeGameplayConfiguration|37computeGameplayReconciliationIdentity")
endif()

if(CUEXIS_SYMBOL_TOOL_KIND STREQUAL "dumpbin")
    set(forbidden_symbols
        RuntimeSession
        AssetDatabase
        entt::
        cuexis::world
        computeFrameDigestVersion
        normalizePresentationFrame
        detail@playback@cuexis)
else()
    set(forbidden_symbols
        RuntimeSession
        AssetDatabase
        4entt
        6cuexis5world
        computeFrameDigestVersion
        normalizePresentationFrame
        6cuexis8playback6detail)
endif()
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
        foreach(forbidden_symbol IN LISTS forbidden_symbols)
            string(FIND "${exported_symbol}" "${forbidden_symbol}" forbidden_position)
            if(NOT forbidden_position EQUAL -1)
                message(FATAL_ERROR
                    "Playback shared library exported internal symbol ${forbidden_symbol}: ${exported_symbol}")
            endif()
        endforeach()
        if(exported_symbol MATCHES "^\\?\\?[014](${public_class_pattern})@" OR
           exported_symbol MATCHES
                "^\\?[^@]+@(${public_class_pattern})@playback@cuexis@@" OR
           exported_symbol MATCHES "^\\?(${public_function_pattern})@playback@cuexis@@")
            continue()
        endif()
    else()
        if(CUEXIS_SYMBOL_TOOL_KIND STREQUAL "objdump")
            # Only the PE ordinal/name pointer table. Address and import rows do not match.
            if(NOT symbol_line MATCHES "${pe_export_name_pattern}")
                continue()
            endif()
            set(exported_symbol "${CMAKE_MATCH_2}")
        else()
            if(NOT symbol_line MATCHES "[ \t][A-Za-z][ \t]+([^ \t]+)$")
                continue()
            endif()
            set(exported_symbol "${CMAKE_MATCH_1}")
        endif()
        if(NOT exported_symbol MATCHES "^_ZN[KVRrO]*6cuexis" AND
           NOT exported_symbol MATCHES "^_ZT[VIS]N6cuexis" AND
           NOT exported_symbol MATCHES "^_ZGVN6cuexis" AND
           NOT exported_symbol MATCHES "^_ZT[hv].*N6cuexis")
            continue()
        endif()
        math(EXPR export_count "${export_count} + 1")
        foreach(forbidden_symbol IN LISTS forbidden_symbols)
            string(FIND "${exported_symbol}" "${forbidden_symbol}" forbidden_position)
            if(NOT forbidden_position EQUAL -1)
                message(FATAL_ERROR
                    "Playback shared library exported internal symbol ${forbidden_symbol}: ${exported_symbol}")
            endif()
        endforeach()
        if(exported_symbol MATCHES
                "^_ZN[KVRrO]*6cuexis8playback(${public_mangled_classes})" OR
           exported_symbol MATCHES "^_ZN6cuexis8playback(${public_mangled_functions})(B5cxx11)?E")
            continue()
        endif()
    endif()
    message(FATAL_ERROR "Playback shared library exported symbol outside the public allowlist: ${exported_symbol}")
endforeach()
if(export_count EQUAL 0)
    message(FATAL_ERROR "Playback shared export inspection did not parse any symbols")
endif()
