if(TARGET freetype)
    return()
endif()
if (MLN_TEXT_SHAPING_HARFBUZZ)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "freetype option")
    set(FT_REQUIRE_BROTLI OFF CACHE BOOL "freetype option")
    set(FT_DISABLE_ZLIB ON CACHE BOOL "freetype option")
    set(FT_REQUIRE_ZLIB OFF CACHE BOOL "freetype option")
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND MLN_LINUX_STATIC_DEPS)
        # When vendoring deps on Linux, disable PNG and BZip2 to prevent
        # freetype from finding system shared libs that would leak into the
        # final shared library. Current MapLibre glyph rasterization does not
        # depend on FreeType's PNG color-glyph or BZip2-compressed font support.
        set(FT_DISABLE_PNG ON CACHE BOOL "freetype option")
        set(FT_REQUIRE_PNG OFF CACHE BOOL "freetype option")
        set(FT_DISABLE_BZIP2 ON CACHE BOOL "freetype option")
        set(FT_REQUIRE_BZIP2 OFF CACHE BOOL "freetype option")
    else()
        # Ensure these aren't stuck from a previous MLN_LINUX_STATIC_DEPS=ON configure
        unset(FT_DISABLE_PNG CACHE)
        unset(FT_REQUIRE_PNG CACHE)
        unset(FT_DISABLE_BZIP2 CACHE)
        unset(FT_REQUIRE_BZIP2 CACHE)
    endif()
    add_subdirectory(vendor/freetype)

    set_target_properties(
        freetype
        PROPERTIES
            INTERFACE_MAPLIBRE_NAME "freetype"
            INTERFACE_MAPLIBRE_URL "https://github.com/freetype/freetype"
            INTERFACE_MAPLIBRE_AUTHOR "David Turner, Robert Wilhelm, Werner Lemberg and FreeType contributors"
            INTERFACE_MAPLIBRE_LICENSE ${PROJECT_SOURCE_DIR}/vendor/freetype/docs/FTL.TXT
    )

    target_include_directories(
        mbgl-core SYSTEM
        PUBLIC vendor/freetype/include
    )
endif()
