# Repository tools are split by their own build switch. The media importer is gated by
# CUEXIS_BUILD_MEDIA_TOOLS alone so that image and audio import never requires shader-tools.

if(CUEXIS_BUILD_DEVELOPER_TOOLS)
    add_subdirectory(cxc_common)
    add_subdirectory(cxc_pack)
    add_subdirectory(cxc_validate)
    add_subdirectory(cxc_unpack)
    add_subdirectory(chart_validator)
    add_subdirectory(chart_migrator)
    if(TARGET cuexis_shader)
        add_subdirectory(asset_importer)
    endif()
endif()

if(CUEXIS_BUILD_MEDIA_TOOLS)
    add_subdirectory(asset_publish)
    add_subdirectory(media_import)
    add_subdirectory(media_importer)
endif()
