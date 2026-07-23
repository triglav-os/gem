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

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_BUILD_RPATH "$ORIGIN")

file(GLOB gem_runtime_font_files CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/src/resources/fonts/*.fnt)

add_custom_target(gem_runtime_fonts ALL
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        ${PROJECT_SOURCE_DIR}/bin/fonts
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${PROJECT_SOURCE_DIR}/bin/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${gem_runtime_font_files}
        ${PROJECT_SOURCE_DIR}/bin/fonts
    COMMENT "Copying GEM runtime fonts into bin/fonts"
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
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PROJECT_SOURCE_DIR}/include ${GEMIX_INCLUDE}
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PROJECT_SOURCE_DIR}/bin/fonts ${GEMIX_SHARE}/fonts
    COMMAND ${CMAKE_COMMAND} -E copy
        ${PROJECT_SOURCE_DIR}/bin/cursors.rsc ${GEMIX_SHARE}/
    COMMAND ${CMAKE_COMMAND} -E copy
        ${PROJECT_SOURCE_DIR}/bin/desktop_icons.rsc ${GEMIX_SHARE}/
    COMMAND ${CMAKE_COMMAND} -E copy
        ${PROJECT_SOURCE_DIR}/bin/clock_numbers.rsc ${GEMIX_SHARE}/
    COMMAND ${CMAKE_COMMAND} -E copy
        ${PROJECT_SOURCE_DIR}/tools/gemix-session.sh ${GEMIX_BIN}/gemix-session
    COMMAND ${CMAKE_COMMAND}
        -DGEMIX_ROOT=${GEMIX_ROOT}
        -P ${PROJECT_SOURCE_DIR}/tools/package-gemix.cmake
    DEPENDS gemd desktop terminal calc_gemd clock_gemd
    COMMENT "Assembling the Gemix Linux deployment tree"
)
