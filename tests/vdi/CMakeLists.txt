# VDI framebuffer and state regression tests.

add_executable(test_vdi
    test_vdi.c
    vdi_test_host.c
    vdi_reference.c
    vdi_optimized.c
    ${PROJECT_SOURCE_DIR}/src/vdi/workstation.c
    ${PROJECT_SOURCE_DIR}/src/vdi/alpha.c
    ${PROJECT_SOURCE_DIR}/src/vdi/input.c
    ${PROJECT_SOURCE_DIR}/src/vdi/misc.c
    ${PROJECT_SOURCE_DIR}/src/vdi/primitives.c
    ${PROJECT_SOURCE_DIR}/src/vdi/raster.c
    ${PROJECT_SOURCE_DIR}/src/vdi/text.c
    ${PROJECT_SOURCE_DIR}/src/vdi/_core.c
    ${PROJECT_SOURCE_DIR}/src/vdi/_cursor.c
    ${PROJECT_SOURCE_DIR}/src/vdi/_fonts.c
    ${PROJECT_SOURCE_DIR}/src/vdi/_surface.c
)

add_dependencies(test_vdi gem_runtime_fonts gem_runtime_cursors)

target_include_directories(test_vdi
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/vdi
        ${PROJECT_SOURCE_DIR}/tests/vdi
)

target_compile_features(test_vdi PRIVATE c_std_11)

target_compile_options(test_vdi
    PRIVATE
        -Wall
        -Wextra
        -pedantic
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_compile_definitions(test_vdi
    PRIVATE
        GEM_FONT_DIR="${PROJECT_SOURCE_DIR}/bin/fonts"
)

target_link_options(test_vdi
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_libraries(test_vdi PRIVATE m)

add_test(NAME test_vdi COMMAND test_vdi)
set_tests_properties(test_vdi PROPERTIES
    ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
)
