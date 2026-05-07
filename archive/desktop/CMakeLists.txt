add_executable(desktop_hosted main.c)

target_include_directories(desktop_hosted
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/gem
)

target_link_libraries(desktop_hosted PRIVATE aes vdi)

set_target_properties(desktop_hosted PROPERTIES
    OUTPUT_NAME desktop_hosted
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

target_compile_features(desktop_hosted PRIVATE c_std_11)

target_compile_options(desktop_hosted
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(desktop_hosted
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(desktop_hosted
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

add_executable(desktop legacy_main.c)

target_include_directories(desktop
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/gem
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(desktop PRIVATE desktop_legacy_core aes)

set_target_properties(desktop PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

target_compile_features(desktop PRIVATE c_std_11)

target_compile_options(desktop
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(desktop
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(desktop
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

add_library(desktop_legacy_core STATIC
    desktop.c
    deskact.c
    deskapp.c
    deskdir.c
    deskfpd.c
    deskfun.c
    deskglob.c
    deskgraf.c
    deskif.c
    deskinf.c
    deskins.c
    deskobj.c
    deskpro.c
    deskrsrc.c
    desksupp.c
    deskwin.c
    gemdos_host.c
    hcsmall.c
    hostshim.c
    optimize.c
)

target_include_directories(desktop_legacy_core
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/gem
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(desktop_legacy_core PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build
)

target_compile_options(desktop_legacy_core
    PRIVATE
        -std=gnu89
        -Wall
        -Wextra
        -Wno-unused-parameter
        -Wno-int-to-pointer-cast
        -Wno-pointer-to-int-cast
        -Wno-incompatible-pointer-types
        -Wno-builtin-declaration-mismatch
)
