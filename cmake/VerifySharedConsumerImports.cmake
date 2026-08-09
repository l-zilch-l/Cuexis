foreach(required_variable IN ITEMS
        CUEXIS_CONSUMER
        CUEXIS_DUMPBIN
        CUEXIS_AUDIO_LIBRARY
        CUEXIS_CORE_LIBRARY
        CUEXIS_CONTENT_LIBRARY
        CUEXIS_PLAYBACK_LIBRARY
        CUEXIS_PLAYBACK_RUNTIME)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

execute_process(
    COMMAND "${CUEXIS_DUMPBIN}" /nologo /imports "${CUEXIS_CONSUMER}"
    RESULT_VARIABLE import_result
    OUTPUT_VARIABLE import_output
    ERROR_VARIABLE import_error)
if(NOT import_result EQUAL 0)
    message(FATAL_ERROR "Shared consumer import inspection failed: ${import_error}")
endif()

string(TOLOWER "${import_output}" import_output_lower)
if(NOT DEFINED CUEXIS_REQUIRE_CONTENT_IMPORT)
    set(CUEXIS_REQUIRE_CONTENT_IMPORT ON)
endif()
set(required_consumer_libraries CUEXIS_CORE_LIBRARY CUEXIS_PLAYBACK_LIBRARY)
if(CUEXIS_REQUIRE_CONTENT_IMPORT)
    list(APPEND required_consumer_libraries CUEXIS_CONTENT_LIBRARY)
endif()
foreach(required_library IN LISTS required_consumer_libraries)
    string(TOLOWER "${${required_library}}" required_library_lower)
    string(FIND "${import_output_lower}" "${required_library_lower}" required_library_offset)
    if(required_library_offset EQUAL -1)
        message(FATAL_ERROR "Shared consumer does not import ${${required_library}}")
    endif()
endforeach()
foreach(forbidden_library IN ITEMS sdl3.dll cuexis_assets cuexis_runtime cuexis_world)
    string(FIND "${import_output_lower}" "${forbidden_library}" forbidden_library_offset)
    if(NOT forbidden_library_offset EQUAL -1)
        message(FATAL_ERROR "Base shared consumer imports ${forbidden_library}")
    endif()
endforeach()

execute_process(
    COMMAND "${CUEXIS_DUMPBIN}" /nologo /imports "${CUEXIS_PLAYBACK_RUNTIME}"
    RESULT_VARIABLE playback_import_result
    OUTPUT_VARIABLE playback_import_output
    ERROR_VARIABLE playback_import_error)
if(NOT playback_import_result EQUAL 0)
    message(FATAL_ERROR "Playback runtime import inspection failed: ${playback_import_error}")
endif()

string(TOLOWER "${playback_import_output}" playback_import_output_lower)
foreach(required_library IN ITEMS
        CUEXIS_AUDIO_LIBRARY
        CUEXIS_CORE_LIBRARY
        CUEXIS_CONTENT_LIBRARY)
    string(TOLOWER "${${required_library}}" required_library_lower)
    string(FIND "${playback_import_output_lower}" "${required_library_lower}"
        required_library_offset)
    if(required_library_offset EQUAL -1)
        message(FATAL_ERROR "Playback runtime does not import ${${required_library}}")
    endif()
endforeach()
foreach(forbidden_library IN ITEMS sdl3.dll glad cuexis_assets cuexis_runtime cuexis_world)
    string(FIND "${playback_import_output_lower}" "${forbidden_library}"
        forbidden_library_offset)
    if(NOT forbidden_library_offset EQUAL -1)
        message(FATAL_ERROR "Playback runtime imports ${forbidden_library}")
    endif()
endforeach()
