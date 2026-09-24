find_package(FLAC CONFIG REQUIRED)
find_package(JPEG REQUIRED)
find_package(Ogg CONFIG REQUIRED)
find_package(PNG REQUIRED)
find_package(Vorbis CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

add_library(cuexis_media_import STATIC
    src/audio_import.cpp
    src/image_import.cpp
    src/media_identity.cpp
    src/minimp3_impl.cpp
)
add_library(cuexis::media_import ALIAS cuexis_media_import)

target_include_directories(cuexis_media_import
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
# minimp3 is header-only and ships in its own include subdirectory.
target_include_directories(cuexis_media_import
    PRIVATE
        ${MINIMP3_INCLUDE_DIRS}
)
target_compile_features(cuexis_media_import PUBLIC cxx_std_20)
target_link_libraries(cuexis_media_import
    PUBLIC
        cuexis::core
    PRIVATE
        FLAC::FLAC
        JPEG::JPEG
        Ogg::ogg
        PNG::PNG
        Vorbis::vorbis
        ZLIB::ZLIB
)

# The scalar minimp3 build is a profile decision: SIMD paths would make canonical bytes depend on
# the target ISA. Fast-math and contraction are forbidden for conversion code.
target_compile_definitions(cuexis_media_import PRIVATE MINIMP3_NO_SIMD=1)
if(MSVC)
    target_compile_options(cuexis_media_import PRIVATE /fp:precise)
else()
    target_compile_options(cuexis_media_import PRIVATE -fno-fast-math -ffp-contract=off)
endif()
