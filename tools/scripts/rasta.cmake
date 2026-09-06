# Build the viewer from a verified upstream snapshot, never a sibling checkout.
include(ExternalProject)
set(GEM_RASTA_REVISION "3dd2426f5c4bddf7d0eb4961d73c2e8c68e3510a")
set(GEM_RASTA_SHA256 "755505af8181467203d8b8a7a572bdb01663573a0739266a405232d6ab20ba86")
ExternalProject_Add(rasta_viewer
    URL "https://codeload.github.com/tstih/rasta/tar.gz/${GEM_RASTA_REVISION}"
    URL_HASH "SHA256=${GEM_RASTA_SHA256}"
    TLS_VERIFY TRUE
    PREFIX "${CMAKE_BINARY_DIR}/deps/rasta"
    DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/deps/downloads"
    SOURCE_DIR "${CMAKE_BINARY_DIR}/deps/rasta-source"
    BINARY_DIR "${CMAKE_BINARY_DIR}/deps/rasta-build"
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc
        -DCMAKE_CXX_COMPILER=g++
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target rasta test_rasta --parallel 4
    INSTALL_COMMAND ${CMAKE_COMMAND} -E make_directory "${GEM_TOOLS_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            <SOURCE_DIR>/bin/rasta "${GEM_TOOLS_DIR}/rasta"
    BUILD_BYPRODUCTS "${GEM_TOOLS_DIR}/rasta")

ExternalProject_Add_Step(rasta_viewer gem_inverse
    COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=<SOURCE_DIR>
        -P ${PROJECT_SOURCE_DIR}/tools/scripts/rasta_inverse.cmake
    DEPENDEES patch
    DEPENDERS configure
    DEPENDS ${PROJECT_SOURCE_DIR}/tools/scripts/rasta_inverse.cmake)
