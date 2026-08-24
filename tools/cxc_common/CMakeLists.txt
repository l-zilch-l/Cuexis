add_library(cuexis_cxc_tool_common STATIC)
add_library(cuexis::cxc_tool_common ALIAS cuexis_cxc_tool_common)

target_sources(cuexis_cxc_tool_common
    PRIVATE
        src/cxc_pack.cpp
        src/cxc_tool_common.cpp
        src/cxc_tool_internal.hpp
        src/cxc_unpack.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
        include/cuexis/tools/cxc_tool.hpp
)

target_compile_features(cuexis_cxc_tool_common PUBLIC cxx_std_20)
target_link_libraries(cuexis_cxc_tool_common
    PUBLIC
        cuexis_cxc
    PRIVATE
        cuexis::filesystem
        cuexis::project
        cuexis::chart
)

if(CUEXIS_BUILD_TESTS)
    target_compile_definitions(cuexis_cxc_tool_common PRIVATE CUEXIS_CXC_TOOL_TESTING=1)
endif()
