file(GLOB demo20_sources CONFIGURE_DEPENDS *.c)

add_executable(demo20 ${demo20_sources})

target_include_directories(demo20
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(demo20 PRIVATE aes vdi)

set_target_properties(demo20 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

target_compile_features(demo20 PRIVATE c_std_11)

target_compile_options(demo20
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(demo20
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(demo20
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
