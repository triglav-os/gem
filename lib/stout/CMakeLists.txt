add_library(libstout STATIC
    traps.c
    disk.c
    disk_write.c
    tos/common.c
    tos/bios.c
    tos/xbios.c
    tos/gemdos.c
)

set_target_properties(libstout PROPERTIES OUTPUT_NAME stout)

target_include_directories(libstout
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(libstout PUBLIC gem prg musashi fat12 msa platform_linux m)

target_compile_features(libstout PUBLIC c_std_11)

target_compile_options(libstout PRIVATE -Wall -Wextra -pedantic
    $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
target_link_options(libstout PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
