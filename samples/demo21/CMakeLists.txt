file(GLOB demo21_sources CONFIGURE_DEPENDS *.c)

add_executable(demo21 ${demo21_sources})

target_include_directories(demo21
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(demo21 PRIVATE aes vdi)

set_target_properties(demo21 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

target_compile_features(demo21 PRIVATE c_std_11)

target_compile_options(demo21
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(demo21
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(demo21
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
