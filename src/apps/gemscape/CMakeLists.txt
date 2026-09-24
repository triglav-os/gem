include(ExternalProject)

set(GEMSCAPE_CEF_VERSION
    "154.0.23+g062ebe4+chromium-154.0.8037.17")
set(GEMSCAPE_CEF_ARCHIVE
    "cef_binary_${GEMSCAPE_CEF_VERSION}_linux64_minimal.tar.bz2")
set(GEMSCAPE_CEF_URL
    "https://cef-builds.spotifycdn.com/${GEMSCAPE_CEF_ARCHIVE}")
set(GEMSCAPE_CEF_ROOT ${CMAKE_BINARY_DIR}/deps/cef/source)
set(GEMSCAPE_CEF_LIB ${GEMSCAPE_CEF_ROOT}/Release/libcef.so)

# CEF is a verified build dependency, not a repository dependency. Its
# archive and extracted runtime stay under build/deps and are reused by
# unchanged builds. ICU and the core resource packs must also sit beside
# libcef.so for Chromium's Linux loader, so generate those copies in the
# extracted build cache rather than in the source tree.
ExternalProject_Add(gemscape_cef
    URL ${GEMSCAPE_CEF_URL}
    URL_HASH
        SHA256=8beed3410b79aa4ae964bb7f955f08ac550eb2a377a6d12090be8bdb7de1369d
    TLS_VERIFY TRUE
    PREFIX ${CMAKE_BINARY_DIR}/deps/cef
    DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/deps/downloads
    SOURCE_DIR ${GEMSCAPE_CEF_ROOT}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND
        ${CMAKE_COMMAND} -E copy_if_different
        <SOURCE_DIR>/Resources/icudtl.dat
        <SOURCE_DIR>/Release/icudtl.dat
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        <SOURCE_DIR>/Resources/chrome_100_percent.pak
        <SOURCE_DIR>/Release/chrome_100_percent.pak
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        <SOURCE_DIR>/Resources/chrome_200_percent.pak
        <SOURCE_DIR>/Release/chrome_200_percent.pak
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        <SOURCE_DIR>/Resources/resources.pak
        <SOURCE_DIR>/Release/resources.pak
    BUILD_BYPRODUCTS
        ${GEMSCAPE_CEF_LIB}
        ${GEMSCAPE_CEF_ROOT}/Release/icudtl.dat
        ${GEMSCAPE_CEF_ROOT}/Release/chrome_100_percent.pak
        ${GEMSCAPE_CEF_ROOT}/Release/chrome_200_percent.pak
        ${GEMSCAPE_CEF_ROOT}/Release/resources.pak)

file(MAKE_DIRECTORY ${GEMSCAPE_CEF_ROOT})
add_library(gemscape_cef_runtime SHARED IMPORTED GLOBAL)
set_target_properties(gemscape_cef_runtime PROPERTIES
    IMPORTED_LOCATION ${GEMSCAPE_CEF_LIB}
    INTERFACE_INCLUDE_DIRECTORIES ${GEMSCAPE_CEF_ROOT})
add_dependencies(gemscape_cef_runtime gemscape_cef)

add_executable(gemscape gemscape.c gemscape_ui.c)
target_compile_definitions(gemscape PRIVATE GEMSCAPE_OVERLAY=1)
target_sources(gemscape PRIVATE cef_browser.c)
target_compile_definitions(gemscape PRIVATE GEMSCAPE_CEF=1
    GEM_CEF_ROOT_PATH="${GEMSCAPE_CEF_ROOT}")
target_link_libraries(gemscape PRIVATE gemscape_cef_runtime dl pthread)
set_target_properties(gemscape PROPERTIES
    BUILD_RPATH "${GEMSCAPE_CEF_ROOT}/Release"
    # The project writes development executables directly to bin/apps while
    # globally enabling install RPATHs. Point that output at the downloaded
    # build cache, never at the former source-tree lib/cef location.
    INSTALL_RPATH "${GEMSCAPE_CEF_ROOT}/Release;$ORIGIN/../lib")

if(TARGET gem_runtime_gemscape_toolbar)
    add_dependencies(gemscape gem_runtime_gemscape_toolbar)
endif()

target_include_directories(gemscape
    PRIVATE
        ${PROJECT_SOURCE_DIR}/samples/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(gemscape PRIVATE gem gemctl)

set_target_properties(gemscape PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${GEM_APPS_DIR}
)

target_compile_features(gemscape PRIVATE c_std_11)

target_compile_options(gemscape
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(gemscape
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(gemscape
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

# The prebuilt CEF runtime is a release binary and must not be mixed with the
# debug sanitizer allocator. Keep CEF-enabled Gemscape debuggable with symbols
# while disabling ASAN/UBSAN instrumentation for this executable.
target_compile_options(gemscape PRIVATE -fno-sanitize=address,undefined)
# libgem.so may still be a sanitizer-instrumented Debug shared library; retain
# the runtime on the final link even though Gemscape itself is not instrumented.
target_link_options(gemscape PRIVATE -fno-sanitize=address,undefined
    $<$<CONFIG:Debug>:-Wl,--no-as-needed;-lasan;-lubsan>)

# The direct variant remains available for isolated display sessions.
add_executable(gemscape_hosted gemscape.c gemscape_ui.c)
target_compile_definitions(gemscape_hosted PRIVATE GEMSCAPE_OVERLAY=0)
target_link_libraries(gemscape_hosted PRIVATE aes vdi gemctl)
target_include_directories(gemscape_hosted PRIVATE
    ${PROJECT_SOURCE_DIR}/samples/include
    ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(gemscape_hosted PRIVATE -Wall -Wextra -pedantic
    $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
target_link_options(gemscape_hosted PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
set_target_properties(gemscape_hosted PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${GEM_APPS_DIR})
