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
        ${PROJECT_SOURCE_DIR}/src/mbgl/skia/layer_group.cpp
        ${PROJECT_SOURCE_DIR}/src/mbgl/skia/renderer_backend.cpp
        ${PROJECT_SOURCE_DIR}/src/mbgl/skia/resources.cpp
        ${PROJECT_SOURCE_DIR}/src/mbgl/shaders/skia/shader_program.cpp
)

target_link_libraries(
        mbgl-core
        PUBLIC
        mbgl-vendor-skia
)
