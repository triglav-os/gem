#
# Builds the upstream Musashi 68k core from the Makefile-based source
# snapshot downloaded into the ignored root build tree. Local safety patches
# are applied before compiling. The opcode tables are generated
# into the build directory so the source tree stays clean.
#
# MIT License (see: LICENSE)
# Copyright (C) 2026 tomaz stih
#

cmake_minimum_required(VERSION 3.20)

include(FetchContent)
find_package(Git REQUIRED)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(musashi_revision 313ebf1bd9f4d0d93341eb5ce21fd8a119e9dbdd)
set(musashi_archive_sha256
    ffddb3f96eba9d4c0f09327a48b5f35398c56fcb8bc79fff5ac352a401dfafcc)
set(musashi_patch ${CMAKE_CURRENT_SOURCE_DIR}/patches/hosted.patch)
file(SHA256 ${musashi_patch} musashi_patch_sha256)
# A changed patch gets a fresh extraction; the verified archive remains cached.
set(musashi_cache_key ${musashi_revision}-${musashi_patch_sha256}-v1)
set(musashi_deps_dir ${CMAKE_BINARY_DIR}/deps)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${musashi_patch})
FetchContent_Declare(gem_musashi
    URL https://codeload.github.com/retro-vault/musashi/tar.gz/${musashi_revision}
    URL_HASH SHA256=${musashi_archive_sha256}
    TLS_VERIFY TRUE
    DOWNLOAD_DIR ${musashi_deps_dir}/downloads
    DOWNLOAD_NAME musashi-${musashi_revision}.tar.gz
    SOURCE_DIR ${musashi_deps_dir}/musashi-${musashi_cache_key}
    BINARY_DIR ${musashi_deps_dir}/musashi-build-${musashi_cache_key}
    SUBBUILD_DIR ${musashi_deps_dir}/musashi-populate-${musashi_cache_key}
    # Keep git apply outside the parent repository's index/worktree context.
    PATCH_COMMAND ${CMAKE_COMMAND} -E env --unset=GIT_DIR --unset=GIT_WORK_TREE
        GIT_CEILING_DIRECTORIES=${musashi_deps_dir}
        ${GIT_EXECUTABLE} apply --whitespace=nowarn ${musashi_patch})
FetchContent_MakeAvailable(gem_musashi)
set(musashi_source_dir ${gem_musashi_SOURCE_DIR})

set(musashi_generated_dir ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(musashi_generated_c ${musashi_generated_dir}/m68kops.c)
set(musashi_generated_h ${musashi_generated_dir}/m68kops.h)

file(MAKE_DIRECTORY ${musashi_generated_dir})

add_executable(m68kmake
    ${musashi_source_dir}/m68kmake.c
)

set_target_properties(m68kmake PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_features(m68kmake PRIVATE c_std_11)

target_compile_options(m68kmake
    PRIVATE
        -Wall
        -Wextra
        -pedantic
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(m68kmake
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

add_custom_command(
    OUTPUT
        ${musashi_generated_c}
        ${musashi_generated_h}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${musashi_generated_dir}
    COMMAND
        $<TARGET_FILE:m68kmake>
        ${musashi_generated_dir}/
        ${musashi_source_dir}/m68k_in.c
    DEPENDS
        m68kmake
        ${musashi_source_dir}/m68k_in.c
        ${musashi_source_dir}/m68kmake.c
    COMMENT "Generating Musashi opcode tables"
    VERBATIM
)

add_library(musashi STATIC
    ${musashi_source_dir}/m68kcpu.c
    ${musashi_source_dir}/m68kdasm.c
    ${musashi_source_dir}/softfloat/softfloat.c
    ${musashi_generated_c}
)

target_include_directories(musashi
    PUBLIC
        ${musashi_source_dir}
    PRIVATE
        ${musashi_generated_dir}
)

target_compile_features(musashi PUBLIC c_std_11)
# Stout implements GEM traps and rejects unsupported guest instructions.
target_compile_definitions(musashi PRIVATE
    M68K_TRAP_HAS_CALLBACK=1
    M68K_ILLG_HAS_CALLBACK=1
)

target_compile_options(musashi
    PRIVATE
        -Wall
        -Wextra
        -pedantic
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(musashi
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
