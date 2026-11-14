# Build the viewer from a verified upstream snapshot, never a sibling checkout.
include(ExternalProject)
find_package(Git REQUIRED)
set(GEM_RASTA_REVISION "5f1e1511e918e8b4dfa6f8b7a51272f66133ea0e")
set(GEM_RASTA_SHA256 "c017b7e0884d8e94bf16a6ce549c62f2fbf2013aeaf3bf14ef1fb0cc4bc04e78")
set(GEM_RASTA_PATCH "${CMAKE_CURRENT_LIST_DIR}/patches/rasta-framebuffer.patch")
file(SHA256 "${GEM_RASTA_PATCH}" GEM_RASTA_PATCH_SHA256)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${GEM_RASTA_PATCH}")
# Patch changes get a fresh extraction; the verified download remains cached.
set(GEM_RASTA_CACHE_KEY "${GEM_RASTA_REVISION}-${GEM_RASTA_PATCH_SHA256}")
set(GEM_RASTA_SOURCE_DIR "${CMAKE_BINARY_DIR}/deps/rasta-source-${GEM_RASTA_CACHE_KEY}")
ExternalProject_Add(rasta_viewer
    URL "https://codeload.github.com/tstih/rasta/tar.gz/${GEM_RASTA_REVISION}"
    URL_HASH "SHA256=${GEM_RASTA_SHA256}"
    TLS_VERIFY TRUE
    PREFIX "${CMAKE_BINARY_DIR}/deps/rasta-${GEM_RASTA_CACHE_KEY}"
    DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/deps/downloads"
    SOURCE_DIR "${GEM_RASTA_SOURCE_DIR}"
    BINARY_DIR "${CMAKE_BINARY_DIR}/deps/rasta-build-${GEM_RASTA_CACHE_KEY}"
    PATCH_COMMAND ${CMAKE_COMMAND} -E env --unset=GIT_DIR --unset=GIT_WORK_TREE
        "GIT_CEILING_DIRECTORIES=${CMAKE_BINARY_DIR}/deps"
        ${GIT_EXECUTABLE} apply --whitespace=nowarn "${GEM_RASTA_PATCH}"
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc
        -DCMAKE_CXX_COMPILER=g++
    # Recheck outputs incrementally, including after bin/ has been removed.
    # Download/configure stamps still preserve the cached upstream snapshot.
    BUILD_ALWAYS TRUE
    BUILD_COMMAND ${CMAKE_COMMAND} -E env --unset=MAKEFLAGS --unset=MFLAGS
        --unset=GNUMAKEFLAGS ${CMAKE_COMMAND} --build <BINARY_DIR>
        --target rasta test_rasta --parallel 4
    INSTALL_COMMAND ${CMAKE_COMMAND} -E make_directory "${GEM_TOOLS_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            <SOURCE_DIR>/bin/rasta "${GEM_TOOLS_DIR}/rasta"
    BUILD_BYPRODUCTS "${GEM_TOOLS_DIR}/rasta")
