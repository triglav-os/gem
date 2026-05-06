file(GLOB vdi_sources CONFIGURE_DEPENDS *.c)

add_library(vdi SHARED ${vdi_sources})

add_dependencies(vdi gem_runtime_fonts gem_runtime_cursors)

target_include_directories(vdi
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(vdi PUBLIC rasta m)

target_compile_features(vdi PUBLIC c_std_11)

target_compile_options(vdi
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_definitions(vdi
    PRIVATE
        GEM_FONT_DIR="${PROJECT_SOURCE_DIR}/bin/fonts"
        GEM_CURSOR_RSC="${PROJECT_SOURCE_DIR}/bin/cursors.rsc"
)

target_compile_options(vdi
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
