include(ExternalProject)

set(GEMREAD_MUPDF_VERSION 1.26.10)
set(GEMREAD_MUPDF_URL
    "https://mupdf.com/downloads/archive/mupdf-${GEMREAD_MUPDF_VERSION}-source.tar.gz")
set(GEMREAD_MUPDF_ROOT ${CMAKE_BINARY_DIR}/deps/mupdf)
set(GEMREAD_MUPDF_SOURCE ${GEMREAD_MUPDF_ROOT}/source)
set(GEMREAD_MUPDF_BUILD ${GEMREAD_MUPDF_SOURCE}/build/release)
set(GEMREAD_MUPDF_DOWNLOAD ${CMAKE_BINARY_DIR}/deps/downloads)
file(MAKE_DIRECTORY ${GEMREAD_MUPDF_SOURCE}/include)

# MuPDF is a build dependency, not a repository dependency. ExternalProject
# keeps its archive, extracted source and objects under build/deps, so a second
# build reuses the download while no vendor files enter Git. MuPDF 1.26.10
# explicitly discards one ftruncate result; disable that warning only for the
# upstream build rather than changing its extracted source.
ExternalProject_Add(gemread_mupdf
    URL ${GEMREAD_MUPDF_URL}
    URL_HASH SHA256=1653f35bd8fbd970f05523efdc7f86e41e9728e2564a3295296e03cf59a51437
    DOWNLOAD_DIR ${GEMREAD_MUPDF_DOWNLOAD}
    SOURCE_DIR ${GEMREAD_MUPDF_SOURCE}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ${CMAKE_COMMAND} -E env --unset=MAKEFLAGS --unset=MFLAGS
        --unset=GNUMAKEFLAGS ${CMAKE_MAKE_PROGRAM} -C <SOURCE_DIR>
        build=release HAVE_X11=no HAVE_GLUT=no HAVE_CURL=no
        HAVE_LEPTONICA=no HAVE_TESSERACT=no HAVE_JS=no
        HAVE_OBJCOPY=no XCFLAGS=-Wno-unused-result libs libmupdf-threads
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS
        ${GEMREAD_MUPDF_BUILD}/libmupdf.a
        ${GEMREAD_MUPDF_BUILD}/libmupdf-third.a
        ${GEMREAD_MUPDF_BUILD}/libmupdf-threads.a
)

add_library(gemread_mupdf_core STATIC IMPORTED GLOBAL)
set_target_properties(gemread_mupdf_core PROPERTIES
    IMPORTED_LOCATION ${GEMREAD_MUPDF_BUILD}/libmupdf.a
    INTERFACE_INCLUDE_DIRECTORIES ${GEMREAD_MUPDF_SOURCE}/include)
add_dependencies(gemread_mupdf_core gemread_mupdf)

add_library(gemread_mupdf_third STATIC IMPORTED GLOBAL)
set_target_properties(gemread_mupdf_third PROPERTIES
    IMPORTED_LOCATION ${GEMREAD_MUPDF_BUILD}/libmupdf-third.a)
add_dependencies(gemread_mupdf_third gemread_mupdf)

add_library(gemread_mupdf_threads STATIC IMPORTED GLOBAL)
set_target_properties(gemread_mupdf_threads PROPERTIES
    IMPORTED_LOCATION ${GEMREAD_MUPDF_BUILD}/libmupdf-threads.a)
add_dependencies(gemread_mupdf_threads gemread_mupdf)

set(GEMREAD_MUPDF_LIBRARIES
    gemread_mupdf_core gemread_mupdf_third
    gemread_mupdf_threads m pthread dl)

function(gemread_configure_target target hosted)
    add_executable(${target} gemread.c)
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR} ${GEMREAD_MUPDF_SOURCE}/include)
    if(${hosted})
        target_link_libraries(${target} PRIVATE aes vdi gemctl
            ${GEMREAD_MUPDF_LIBRARIES})
        target_compile_definitions(${target} PRIVATE GEMREAD_HOSTED=1
            GEMREAD_OVERLAY=0)
    else()
        target_link_libraries(${target} PRIVATE gem gemctl
            ${GEMREAD_MUPDF_LIBRARIES})
        target_compile_definitions(${target} PRIVATE GEMREAD_OVERLAY=1)
    endif()
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${GEM_APPS_DIR})
    target_compile_features(${target} PRIVATE c_std_11)
    target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic
        $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
    target_link_options(${target} PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
endfunction()

gemread_configure_target(gemread FALSE)
gemread_configure_target(gemread_hosted TRUE)
