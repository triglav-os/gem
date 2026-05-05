file(GLOB vdi_sources CONFIGURE_DEPENDS *.c)

add_library(vdi STATIC ${vdi_sources})

add_dependencies(vdi gem_runtime_fonts)

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
)

target_compile_options(vdi
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(vdi
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
