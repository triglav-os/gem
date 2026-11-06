cmake_minimum_required(VERSION 3.20)

if(NOT "$ENV{GEM_IN_CONTAINER}" STREQUAL "1")
    message(FATAL_ERROR "Use make or tools/scripts/container.sh exec cmake; builds require Docker")
endif()

project(gem C)

include(CTest)
include(GNUInstallDirs)
find_package(Threads REQUIRED)

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
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(GEM_OUTPUT_ROOT ${PROJECT_SOURCE_DIR}/bin CACHE PATH
    "Runtime output root (use a separate tree for backend diagnostics)")
set(GEM_CORE_DIR ${GEM_OUTPUT_ROOT}/core)
set(GEM_LIBRARY_DIR ${GEM_OUTPUT_ROOT}/lib)
set(GEM_APPS_DIR ${GEM_OUTPUT_ROOT}/apps)
set(GEM_SAMPLES_DIR ${GEM_OUTPUT_ROOT}/samples)
set(GEM_DEMOS_DIR ${GEM_OUTPUT_ROOT}/demos)
set(GEM_RESOURCES_DIR ${GEM_OUTPUT_ROOT}/resources)
set(GEM_TOOLS_DIR ${GEM_OUTPUT_ROOT}/tools)

file(MAKE_DIRECTORY
    ${GEM_CORE_DIR}
    ${GEM_LIBRARY_DIR}
    ${GEM_APPS_DIR}
    ${GEM_SAMPLES_DIR}
    ${GEM_DEMOS_DIR}
    ${GEM_RESOURCES_DIR}
    ${GEM_TOOLS_DIR}
)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${GEM_LIBRARY_DIR})
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_BUILD_RPATH "${GEM_LIBRARY_DIR}")
set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")

file(GLOB gem_runtime_font_files CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/src/resources/fonts/*.fnt)

set(gem_font_outputs)
foreach(font IN LISTS gem_runtime_font_files)
    get_filename_component(name "${font}" NAME)
    set(output "${GEM_RESOURCES_DIR}/fonts/${name}")
    add_custom_command(OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GEM_RESOURCES_DIR}/fonts"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${font}" "${output}"
        DEPENDS "${font}" VERBATIM)
    list(APPEND gem_font_outputs "${output}")
endforeach()
add_custom_target(gem_runtime_fonts ALL DEPENDS ${gem_font_outputs})

if(GEM_PLATFORM STREQUAL "rasta" OR BUILD_TESTING)
    include(tools/scripts/rasta.cmake)
endif()

add_subdirectory(lib/platform/rasta)
add_subdirectory(lib/platform/linux)
add_subdirectory(lib/gemconf)
add_subdirectory(lib/gemctl)
add_subdirectory(lib/musashi)
add_subdirectory(lib/fat12)
add_subdirectory(lib/msa)
add_subdirectory(lib/prg)
add_subdirectory(lib/stout)
add_subdirectory(tools/resgen)
add_subdirectory(src/vdi)
add_subdirectory(src/aes)
add_subdirectory(src/gem)
add_subdirectory(src/gemd)
if(EXISTS ${PROJECT_SOURCE_DIR}/src/rcs/CMakeLists.txt)
    add_subdirectory(src/rcs)
endif()
add_subdirectory(samples)
add_subdirectory(src/apps)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

include(tools/scripts/install.cmake)

set(gem_sdk_libraries gem aes vdi gemconf gemctl fat12 msa platform_linux)
if(GEM_PLATFORM STREQUAL "rasta")
    list(APPEND gem_sdk_libraries rasta)
endif()
set(gem_sdk_library_files)
foreach(library IN LISTS gem_sdk_libraries)
    list(APPEND gem_sdk_library_files "$<TARGET_FILE:${library}>")
endforeach()
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/sdk-export.cmake CONTENT
"if(NOT DEFINED SDK_ROOT)
  set(SDK_ROOT \"${GEM_OUTPUT_ROOT}/sdk\")
endif()
set(GEM_PLATFORM ${GEM_PLATFORM})
set(SOURCE_ROOT \"${PROJECT_SOURCE_DIR}\")
set(RESOURCE_ROOT \"${GEM_RESOURCES_DIR}\")
set(RESGEN \"$<TARGET_FILE:resgen>\")
set(GEM_LIBRARIES \"${gem_sdk_library_files}\")
include(\"${PROJECT_SOURCE_DIR}/tools/scripts/gem_sdk.cmake\")
")
add_custom_target(gem_sdk
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/sdk-export.cmake
    DEPENDS ${gem_sdk_libraries} gem_runtime_fonts gem_runtime_cursors
        gem_runtime_alert_icons resgen
    COMMENT "Exporting the public GEM SDK"
    VERBATIM)
