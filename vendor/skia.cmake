# Skia renderer dependency configuration.
#
# Skia is built with its native GN/Ninja build. The source checkout is placed in
# vendor/skia so local builds can reuse it, matching the pattern used by Dawn.

if(TARGET mbgl-vendor-skia)
    return()
endif()

if(NOT MLN_WITH_SKIA)
    return()
endif()

include(ExternalProject)

set(MLN_SKIA_GIT_VERSION "6b4167b4e2045e0bb559d903f4adea4b42dc6ccb" CACHE STRING "Git ref used when fetching Skia")
set(MLN_SKIA_ENABLE_GPU OFF CACHE BOOL "Build Skia with Ganesh GPU backends enabled")

message(STATUS "Configuring Skia dependency (${MLN_SKIA_GIT_VERSION})")

set(_mln_skia_source_dir "${PROJECT_SOURCE_DIR}/vendor/skia")
set(_mln_skia_build_dir "${CMAKE_BINARY_DIR}/vendor/skia/out")
set(_mln_skia_library "${_mln_skia_build_dir}/libskia.a")

find_program(MLN_GN_EXECUTABLE gn REQUIRED)
find_program(MLN_NINJA_EXECUTABLE ninja REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(_mln_skia_is_debug "false")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_mln_skia_is_debug "true")
endif()

set(_mln_skia_enable_gpu "false")
if(MLN_SKIA_ENABLE_GPU)
    set(_mln_skia_enable_gpu "true")
endif()

set(_mln_skia_target_cpu "")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    set(_mln_skia_target_cpu "arm64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    set(_mln_skia_target_cpu "x64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86|x86")
    set(_mln_skia_target_cpu "x86")
endif()

set(_mln_skia_target_os "")
if(APPLE)
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(_mln_skia_target_os "ios")
    else()
        set(_mln_skia_target_os "mac")
    endif()
elseif(ANDROID)
    set(_mln_skia_target_os "android")
elseif(WIN32)
    set(_mln_skia_target_os "win")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_mln_skia_target_os "linux")
endif()

set(_mln_skia_gn_args
    "is_official_build=false"
    "is_component_build=false"
    "is_debug=${_mln_skia_is_debug}"
    "skia_enable_tools=false"
    "skia_enable_gpu=${_mln_skia_enable_gpu}"
    "skia_enable_ganesh=${_mln_skia_enable_gpu}"
    "skia_enable_graphite=${_mln_skia_enable_gpu}"
    "skia_use_dawn=false"
    "skia_use_gl=${_mln_skia_enable_gpu}"
    "skia_use_metal=false"
    "skia_use_vulkan=false"
    "skia_use_fontconfig=false"
    "skia_use_freetype=false"
    "skia_use_harfbuzz=false"
    "skia_use_icu=false"
    "skia_use_libjpeg_turbo_decode=false"
    "skia_use_libjpeg_turbo_encode=false"
    "skia_use_libpng_decode=false"
    "skia_use_libpng_encode=false"
    "skia_use_libwebp_decode=false"
    "skia_use_libwebp_encode=false"
    "skia_use_zlib=false"
    "skia_use_expat=false"
    "skia_use_wuffs=false"
    "skia_use_piex=false"
    "skia_use_perfetto=false"
    "skia_build_rust_targets=false"
)

if(_mln_skia_target_cpu)
    list(APPEND _mln_skia_gn_args "target_cpu=\"${_mln_skia_target_cpu}\"")
endif()
if(_mln_skia_target_os)
    list(APPEND _mln_skia_gn_args "target_os=\"${_mln_skia_target_os}\"")
endif()

if(CMAKE_C_COMPILER)
    list(APPEND _mln_skia_gn_args "cc=\"${CMAKE_C_COMPILER}\"")
endif()
if(CMAKE_CXX_COMPILER)
    list(APPEND _mln_skia_gn_args "cxx=\"${CMAKE_CXX_COMPILER}\"")
endif()

file(MAKE_DIRECTORY ${_mln_skia_build_dir})
string(REPLACE ";" "\n" _mln_skia_gn_args_file_contents "${_mln_skia_gn_args}")
file(WRITE "${_mln_skia_build_dir}/args.gn" "${_mln_skia_gn_args_file_contents}\n")

ExternalProject_Add(maplibre_skia
    GIT_REPOSITORY https://skia.googlesource.com/skia.git
    GIT_TAG ${MLN_SKIA_GIT_VERSION}
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    SOURCE_DIR ${_mln_skia_source_dir}
    BINARY_DIR ${_mln_skia_build_dir}
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E chdir ${_mln_skia_source_dir} ${Python3_EXECUTABLE} tools/git-sync-deps
              COMMAND ${CMAKE_COMMAND} -E chdir ${_mln_skia_source_dir} ${MLN_GN_EXECUTABLE} gen ${_mln_skia_build_dir}
    BUILD_COMMAND ${MLN_NINJA_EXECUTABLE} -C ${_mln_skia_build_dir} skia
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS ${_mln_skia_library}
    USES_TERMINAL_DOWNLOAD TRUE
    USES_TERMINAL_CONFIGURE TRUE
    USES_TERMINAL_BUILD TRUE
)

add_library(mbgl-vendor-skia INTERFACE)
add_dependencies(mbgl-vendor-skia maplibre_skia)

set_target_properties(mbgl-vendor-skia PROPERTIES
    INTERFACE_MAPLIBRE_NAME "Skia"
    INTERFACE_MAPLIBRE_URL "https://skia.googlesource.com/skia"
    INTERFACE_MAPLIBRE_AUTHOR "Google"
    INTERFACE_MAPLIBRE_LICENSE "${_mln_skia_source_dir}/LICENSE"
)

target_link_libraries(mbgl-vendor-skia INTERFACE ${_mln_skia_library})

target_include_directories(mbgl-vendor-skia
    SYSTEM INTERFACE
        ${_mln_skia_source_dir}
        ${_mln_skia_source_dir}/include
)

target_compile_definitions(mbgl-vendor-skia INTERFACE MLN_SKIA_HAS_VENDOR=1)
