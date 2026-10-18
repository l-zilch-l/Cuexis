# Repository tools are split by their own build switch. The media importer is gated by
# CUEXIS_BUILD_MEDIA_TOOLS alone so that image and audio import never requires shader-tools.

if(CUEXIS_BUILD_DEVELOPER_TOOLS OR CUEXIS_BUILD_MEDIA_TOOLS)
    add_subdirectory(common/asset_publish)
endif()
if(CUEXIS_BUILD_DEVELOPER_TOOLS)
    add_subdirectory(gameplay/gameplay_assembler)
    add_subdirectory(cxc/cxc_common)
    add_subdirectory(cxc/cxc_pack)
    add_subdirectory(cxc/cxc_validate)
    add_subdirectory(cxc/cxc_unpack)
    add_subdirectory(chart/chart_candidate)
    add_subdirectory(chart/chart_validator)
    add_subdirectory(chart/chart_migrator)
    if(TARGET cuexis_shader)
        add_subdirectory(shader/asset_importer)
    endif()
endif()

if(CUEXIS_BUILD_MEDIA_TOOLS)
    add_subdirectory(media/media_import)
    add_subdirectory(media/media_importer)
endif()
