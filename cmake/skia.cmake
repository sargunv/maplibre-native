if(NOT MLN_WITH_SKIA)
    return()
endif()

message(STATUS "Configuring experimental Skia renderer backend")

include(${PROJECT_SOURCE_DIR}/vendor/skia.cmake)

target_compile_definitions(
        mbgl-core
        PUBLIC
        MLN_RENDER_BACKEND_SKIA=1
)

list(APPEND
        INCLUDE_FILES
        ${PROJECT_SOURCE_DIR}/include/mbgl/skia/context.hpp
        ${PROJECT_SOURCE_DIR}/include/mbgl/skia/renderer_backend.hpp
        ${PROJECT_SOURCE_DIR}/include/mbgl/shaders/skia/shader_group.hpp
        ${PROJECT_SOURCE_DIR}/include/mbgl/shaders/skia/shader_program.hpp
)

list(APPEND
        SRC_FILES
        ${PROJECT_SOURCE_DIR}/src/mbgl/skia/command_encoder.cpp
        ${PROJECT_SOURCE_DIR}/src/mbgl/skia/context.cpp
        ${PROJECT_SOURCE_DIR}/src/mbgl/skia/drawable.cpp
        ${PROJECT_SOURCE_DIR}/src/mbgl/skia/drawable_builder.cpp
        ${PROJECT_SOURCE_DIR}/src/mbgl/skia/headless_backend.cpp
        ${PROJECT_SOURCE_DIR}/src/mbgl/skia/renderer_backend.cpp
        ${PROJECT_SOURCE_DIR}/src/mbgl/skia/layer_group.cpp
        ${PROJECT_SOURCE_DIR}/src/mbgl/skia/resources.cpp
        ${PROJECT_SOURCE_DIR}/src/mbgl/shaders/skia/shader_program.cpp
)

if(APPLE)
    list(APPEND SRC_FILES ${PROJECT_SOURCE_DIR}/src/mbgl/skia/gpu_context_metal.mm)
else()
    list(APPEND SRC_FILES ${PROJECT_SOURCE_DIR}/src/mbgl/skia/gpu_context.cpp)
endif()

target_link_libraries(
        mbgl-core
        PUBLIC
        mbgl-vendor-skia
)

target_include_directories(
        mbgl-core
        PRIVATE
        ${PROJECT_SOURCE_DIR}/platform/default/include
)

if(APPLE)
    target_link_libraries(
            mbgl-core
            PUBLIC
            "-framework Metal"
            "-framework QuartzCore"
            "-framework Foundation"
            "-framework CoreGraphics"
            "-framework CoreText"
    )
endif()

add_executable(
        mbgl-skia-smoke
        ${PROJECT_SOURCE_DIR}/src/mbgl/skia/smoke.cpp
)

add_executable(
        mbgl-skia-map-smoke
        ${PROJECT_SOURCE_DIR}/src/mbgl/skia/map_smoke.cpp
        ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/gfx/headless_backend.cpp
        ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/i18n/collator.cpp
        ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/i18n/number_format.cpp
        ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/layermanager/layer_manager.cpp
        ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/platform/time.cpp
        ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/text/bidi.cpp
        ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/text/local_glyph_rasterizer.cpp
        ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/util/async_task.cpp
        ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/util/image.cpp
        ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/util/jpeg_reader.cpp
        ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/util/logging_stderr.cpp
        ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/util/monotonic_timer.cpp
        ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/util/png_reader.cpp
        ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/util/run_loop.cpp
        ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/util/string_stdlib.cpp
        ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/util/thread.cpp
        ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/util/thread_local.cpp
        ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/util/timer.cpp
        ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/util/utf.cpp
        ${PROJECT_SOURCE_DIR}/platform/default/src/mbgl/util/webp_reader.cpp
        ${PROJECT_SOURCE_DIR}/vendor/nunicode/src/libnu/ducet.c
        ${PROJECT_SOURCE_DIR}/vendor/nunicode/src/libnu/strcoll.c
        ${PROJECT_SOURCE_DIR}/vendor/nunicode/src/libnu/strings.c
        ${PROJECT_SOURCE_DIR}/vendor/nunicode/src/libnu/tolower.c
        ${PROJECT_SOURCE_DIR}/vendor/nunicode/src/libnu/tounaccent.c
        ${PROJECT_SOURCE_DIR}/vendor/nunicode/src/libnu/toupper.c
        ${PROJECT_SOURCE_DIR}/vendor/nunicode/src/libnu/utf8.c
)

get_target_property(MLN_SKIA_MAP_SMOKE_PRIVATE_LIBRARIES mbgl-core LINK_LIBRARIES)

target_link_libraries(
        mbgl-skia-smoke
        PRIVATE
        mbgl-core
)

target_include_directories(
        mbgl-skia-smoke
        PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(
        mbgl-skia-map-smoke
        PRIVATE
        ${MLN_SKIA_MAP_SMOKE_PRIVATE_LIBRARIES}
        mbgl-compiler-options
        mbgl-core
        uv
        icui18n
        icuuc
        icudata
        png
        jpeg
        webp
)

target_include_directories(
        mbgl-skia-map-smoke
        PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/platform/default/include
        ${PROJECT_SOURCE_DIR}/vendor/nunicode/include
)

target_compile_definitions(
        mbgl-skia-map-smoke
        PRIVATE
        NU_BUILD_STATIC
)

set_property(TARGET mbgl-skia-smoke PROPERTY FOLDER MapLibre)
set_property(TARGET mbgl-skia-map-smoke PROPERTY FOLDER MapLibre)
