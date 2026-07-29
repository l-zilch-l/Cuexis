foreach(required_variable IN ITEMS
        CUEXIS_CONSUMER
        CUEXIS_DUMPBIN
        CUEXIS_CORE_LIBRARY
        CUEXIS_CONTENT_LIBRARY
        CUEXIS_PLAYBACK_LIBRARY)
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
foreach(required_library IN ITEMS
        CUEXIS_CORE_LIBRARY
        CUEXIS_CONTENT_LIBRARY
        CUEXIS_PLAYBACK_LIBRARY)
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
