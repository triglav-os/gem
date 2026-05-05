file(GLOB demo2_sources CONFIGURE_DEPENDS *.c)

add_executable(demo2 ${demo2_sources})

target_include_directories(demo2
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(demo2 PRIVATE vdi)

set_target_properties(demo2 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

target_compile_features(demo2 PRIVATE c_std_11)

target_compile_options(demo2
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(demo2
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(demo2
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
