cmake_minimum_required(VERSION 3.20)

project(gem C)

include(CTest)

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
add_subdirectory(tools/resgen)
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
