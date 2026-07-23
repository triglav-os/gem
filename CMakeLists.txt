cmake_minimum_required(VERSION 3.20)

project(gem C)

include(CTest)

set(GEM_PLATFORM "rasta" CACHE STRING
    "Display/input backend used by VDI (rasta or linux)")
set_property(CACHE GEM_PLATFORM PROPERTY STRINGS rasta linux)
if(NOT GEM_PLATFORM STREQUAL "rasta" AND
   NOT GEM_PLATFORM STREQUAL "linux")
    message(FATAL_ERROR
        "GEM_PLATFORM must be either 'rasta' or 'linux'")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(GEM_OUTPUT_ROOT ${PROJECT_SOURCE_DIR}/bin)
set(GEM_CORE_DIR ${GEM_OUTPUT_ROOT}/core)
set(GEM_LIBRARY_DIR ${GEM_OUTPUT_ROOT}/lib)
set(GEM_APPS_DIR ${GEM_OUTPUT_ROOT}/apps)
set(GEM_DEMOS_DIR ${GEM_OUTPUT_ROOT}/demos)
set(GEM_RESOURCES_DIR ${GEM_OUTPUT_ROOT}/resources)
set(GEM_TOOLS_DIR ${GEM_OUTPUT_ROOT}/tools)

file(MAKE_DIRECTORY
    ${GEM_CORE_DIR}
    ${GEM_LIBRARY_DIR}
    ${GEM_APPS_DIR}
    ${GEM_DEMOS_DIR}
    ${GEM_RESOURCES_DIR}
    ${GEM_TOOLS_DIR}
)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${GEM_LIBRARY_DIR})
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")

file(GLOB gem_runtime_font_files CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/src/resources/fonts/*.fnt)

add_custom_target(gem_runtime_fonts ALL
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        ${GEM_RESOURCES_DIR}/fonts
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${GEM_RESOURCES_DIR}/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${gem_runtime_font_files}
        ${GEM_RESOURCES_DIR}/fonts
    COMMENT "Copying GEM runtime fonts into bin/resources/fonts"
)

add_subdirectory(lib/platform/rasta)
add_subdirectory(lib/platform/linux)
add_subdirectory(lib/musashi)
add_subdirectory(lib/fat12)
add_subdirectory(lib/msa)
add_subdirectory(lib/prg)
add_subdirectory(tools/resgen)
add_subdirectory(tools/msa)
add_subdirectory(tools/stout)
add_subdirectory(src/vdi)
add_subdirectory(src/aes)
add_subdirectory(src/gem)
add_subdirectory(src/gemd)
if(EXISTS ${PROJECT_SOURCE_DIR}/src/rcs/CMakeLists.txt)
    add_subdirectory(src/rcs)
endif()
add_subdirectory(src/desktop)
add_subdirectory(src/apps)
add_subdirectory(samples)
add_subdirectory(tests)

set(GEMIX_ROOT ${PROJECT_SOURCE_DIR}/bin/gemix)
set(GEMIX_BIN ${GEMIX_ROOT}/bin)
set(GEMIX_LIB ${GEMIX_ROOT}/lib)
set(GEMIX_INCLUDE ${GEMIX_ROOT}/include)
set(GEMIX_SHARE ${GEMIX_ROOT}/share/gem)

add_custom_target(gemix_package
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${GEMIX_ROOT}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GEMIX_BIN}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GEMIX_LIB}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GEMIX_INCLUDE}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GEMIX_SHARE}/fonts
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:gemd> ${GEMIX_BIN}/
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:desktop> ${GEMIX_BIN}/
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:terminal> ${GEMIX_BIN}/
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:calc_gemd> ${GEMIX_BIN}/
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:clock_gemd> ${GEMIX_BIN}/
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:gem> ${GEMIX_LIB}/
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:platform_linux> ${GEMIX_LIB}/
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:aes> ${GEMIX_LIB}/
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:vdi> ${GEMIX_LIB}/
    COMMAND ${CMAKE_COMMAND} -E copy
        ${PROJECT_SOURCE_DIR}/include/gem.h ${GEMIX_INCLUDE}/gem.h
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GEMIX_INCLUDE}/gem
    COMMAND ${CMAKE_COMMAND} -E copy
        ${PROJECT_SOURCE_DIR}/include/gem/gem.h ${GEMIX_INCLUDE}/gem/
    COMMAND ${CMAKE_COMMAND} -E copy
        ${PROJECT_SOURCE_DIR}/include/gem/aes.h ${GEMIX_INCLUDE}/gem/
    COMMAND ${CMAKE_COMMAND} -E copy
        ${PROJECT_SOURCE_DIR}/include/gem/vdi.h ${GEMIX_INCLUDE}/gem/
    COMMAND ${CMAKE_COMMAND} -E copy
        ${PROJECT_SOURCE_DIR}/include/gem/portab.h ${GEMIX_INCLUDE}/gem/
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${GEM_RESOURCES_DIR}/fonts ${GEMIX_SHARE}/fonts
    COMMAND ${CMAKE_COMMAND} -E copy
        ${GEM_RESOURCES_DIR}/cursors.rsc ${GEMIX_SHARE}/
    COMMAND ${CMAKE_COMMAND} -E copy
        ${GEM_RESOURCES_DIR}/desktop_icons.rsc ${GEMIX_SHARE}/
    COMMAND ${CMAKE_COMMAND} -E copy
        ${GEM_RESOURCES_DIR}/clock_numbers.rsc ${GEMIX_SHARE}/
    COMMAND ${CMAKE_COMMAND}
        -DGEMIX_ROOT=${GEMIX_ROOT}
        -P ${PROJECT_SOURCE_DIR}/tools/package-gemix.cmake
    DEPENDS gemd desktop terminal calc_gemd clock_gemd
    COMMENT "Assembling the Gemix Linux deployment tree"
)
