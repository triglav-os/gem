file(GLOB demo1_sources CONFIGURE_DEPENDS *.c)

add_executable(demo1 ${demo1_sources})

target_include_directories(demo1
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(demo1 PRIVATE vdi)

set_target_properties(demo1 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

target_compile_features(demo1 PRIVATE c_std_11)

target_compile_options(demo1
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(demo1
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(demo1
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
